// LwIP netif glue for STM32H7 ETH peripheral (RMII, polling NO_SYS mode).
//
// RX path:  HAL calls RxAllocateCallback to get a DMA buffer, then calls
//           RxLinkCallback with the received bytes.  ethernetif_input() polls
//           HAL_ETH_ReadData() which drives both callbacks and returns a pbuf
//           chain ready for netif->input().
//
// TX path:  low_level_output() copies the pbuf chain into a static RAM_D2
//           bounce buffer and calls HAL_ETH_Transmit().  The ETH DMA cannot
//           reach DTCMRAM where LwIP pbuf payloads live, so the copy is
//           mandatory.  HAL_ETH_TxFreeCallback() frees the pbuf ref after DMA
//           completes.
//
// ETH DMA descriptor tables and static RX/TX buffers are placed in RAM_D2 via
// the linker-script sections .RxDecripSection / .TxDecripSection /
// .RxArraySection / .TxScratchSection so the ETH DMA can reach them.

#include <cstring>

#include "stm32h7xx_hal.h"

#include "lwip/err.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "netif/etharp.h"

// ---- ETH handle (shared with stm32h7xx_it.cpp and main.cpp) ----------------

ETH_HandleTypeDef heth;

// ---- DMA descriptor tables and static RX/TX buffers in RAM_D2 --------------

namespace
{

constexpr uint8_t kMacAddr0 {0x02U};
constexpr uint8_t kMacAddr1 {0x00U};
constexpr uint8_t kMacAddr2 {0x00U};
constexpr uint8_t kMacAddr3 {0x00U};
constexpr uint8_t kMacAddr4 {0x00U};
constexpr uint8_t kMacAddr5 {0x01U};

// 32-byte alignment matches the M7 D-cache line.
ETH_DMADescTypeDef DMARxDscrTab[ETH_RX_DESC_CNT]
    __attribute__((section(".RxDecripSection"), aligned(32)));

ETH_DMADescTypeDef DMATxDscrTab[ETH_TX_DESC_CNT]
    __attribute__((section(".TxDecripSection"), aligned(32)));

// One static buffer per RX descriptor.  ETH DMA writes directly into these.
uint8_t RxBuff[ETH_RX_DESC_CNT][ETH_MAX_PACKET_SIZE]
    __attribute__((section(".RxArraySection"), aligned(32)));

// TX bounce buffer.  LwIP pbuf payloads are in DTCMRAM (0x20000000) which the
// ETH DMA bus master cannot access.  Every outgoing frame is flattened here
// (RAM_D2) before being handed to HAL_ETH_Transmit.  Fix #3: eliminates the
// silent DMA bus error that causes HAL_ETH_Transmit to time out and then
// triggers a second pbuf_free via the callback after the timeout path already
// called pbuf_free — corrupting the LwIP pbuf heap.
uint8_t s_tx_scratch[ETH_MAX_PACKET_SIZE]
    __attribute__((section(".TxScratchSection"), aligned(32)));

// Which RxBuff slot to offer to the HAL next.
uint32_t rx_alloc_index {0U};

// Tracks whether HAL_ETH_Start has been called (set after link-up is detected).
bool s_eth_started {false};

// ---- low_level_init --------------------------------------------------------

void low_level_init(struct netif* const netif)
{
    static uint8_t mac_addr[6U] = {
        kMacAddr0, kMacAddr1, kMacAddr2,
        kMacAddr3, kMacAddr4, kMacAddr5
    };

    heth.Instance            = ETH;
    heth.Init.MACAddr        = mac_addr;
    heth.Init.MediaInterface = HAL_ETH_RMII_MODE;
    heth.Init.TxDesc         = DMATxDscrTab;
    heth.Init.RxDesc         = DMARxDscrTab;
    heth.Init.RxBuffLen      = ETH_MAX_PACKET_SIZE;

    HAL_ETH_Init(&heth);

    netif->hwaddr_len = ETH_HWADDR_LEN;
    netif->hwaddr[0]  = kMacAddr0;
    netif->hwaddr[1]  = kMacAddr1;
    netif->hwaddr[2]  = kMacAddr2;
    netif->hwaddr[3]  = kMacAddr3;
    netif->hwaddr[4]  = kMacAddr4;
    netif->hwaddr[5]  = kMacAddr5;

    netif->mtu = 1500U;
    // Fix #2: do NOT set NETIF_FLAG_LINK_UP directly.  Calling the flag directly
    // at init makes netif_set_up() fire etharp_gratuitous() → HAL_ETH_Transmit
    // with no cable attached, which times out and hangs init.  Instead,
    // ethernetif_poll_phy() calls netif_set_link_up() via the LwIP API only
    // once the PHY reports physical link.
    netif->flags = static_cast<uint8_t>(
        NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET);
}

// ---- low_level_output ------------------------------------------------------

err_t low_level_output(struct netif* /*netif*/, struct pbuf* const p)
{
    // Fix #3: flatten the pbuf chain into s_tx_scratch (RAM_D2) before
    // transmission.  Passing q->payload pointers directly would give the ETH
    // DMA a DTCMRAM address it cannot reach, causing a silent bus error and
    // HAL_ETH_Transmit timeout.  After timeout the caller's pbuf_free and the
    // deferred TxFreeCallback would both fire, corrupting the pbuf heap.
    uint32_t frame_len {0U};
    for (struct pbuf* q = p; q != nullptr; q = q->next)
    {
        if (frame_len + static_cast<uint32_t>(q->len)
                > static_cast<uint32_t>(ETH_MAX_PACKET_SIZE))
        {
            return ERR_BUF;
        }
        std::memcpy(&s_tx_scratch[frame_len], q->payload, q->len);
        frame_len += q->len;
    }

    ETH_BufferTypeDef tx_buf {};
    tx_buf.buffer = s_tx_scratch;
    tx_buf.len    = frame_len;
    tx_buf.next   = nullptr;

    ETH_TxPacketConfigTypeDef tx_cfg {};
    tx_cfg.Attributes = ETH_TX_PACKETS_FEATURES_CRCPAD;
    tx_cfg.CRCPadCtrl = ETH_CRC_PAD_INSERT;
    tx_cfg.Length     = frame_len;
    tx_cfg.TxBuffer   = &tx_buf;
    tx_cfg.pData      = p;  // returned to HAL_ETH_TxFreeCallback after DMA completes

    pbuf_ref(p);  // HAL_ETH_TxFreeCallback calls pbuf_free, balancing this ref

    if (HAL_ETH_Transmit(&heth, &tx_cfg, 100U) != HAL_OK)
    {
        pbuf_free(p);  // undo pbuf_ref; caller retains its own reference
        return ERR_IF;
    }

    return ERR_OK;
}

}  // namespace

// ---- Public API (called from main loop) ------------------------------------

extern "C"
{

err_t ethernetif_init(struct netif* const netif)
{
    netif->name[0]    = 'e';
    netif->name[1]    = '0';
    netif->output     = etharp_output;
    netif->linkoutput = low_level_output;

    low_level_init(netif);
    return ERR_OK;
}

void ethernetif_input(struct netif* const netif)
{
    struct pbuf* p = nullptr;

    do
    {
        p = nullptr;
        if (HAL_ETH_ReadData(&heth, reinterpret_cast<void**>(&p)) == HAL_OK
            && p != nullptr)
        {
            if (netif->input(p, netif) != ERR_OK)
            {
                pbuf_free(p);
            }
        }
    }
    while (p != nullptr);
}

// Fix #2: poll the LAN8742 PHY via MDIO and update LwIP link state via API.
// Call from the main loop periodically (e.g. every 500 ms).
// PHY address 0, register 1 = Basic Status Register; bit 2 = Link Status.
// BSR bit 2 is latch-low — read twice so a previous transient drop is cleared.
void ethernetif_poll_phy(struct netif* const netif)
{
    if (heth.gState == HAL_ETH_STATE_RESET) { return; }

    uint32_t bsr {0U};
    if (HAL_ETH_ReadPHYRegister(&heth, 0U, 1U, &bsr) != HAL_OK) { return; }
    if (HAL_ETH_ReadPHYRegister(&heth, 0U, 1U, &bsr) != HAL_OK) { return; }

    const bool phy_link_up = (bsr & (1U << 2U)) != 0U;

    if (phy_link_up && !s_eth_started)
    {
        HAL_ETH_Start(&heth);
        s_eth_started = true;
    }

    if (netif == nullptr) { return; }

    if (phy_link_up && !netif_is_link_up(netif))
    {
        netif_set_link_up(netif);
        netif_set_up(netif);
    }
    else if (!phy_link_up && netif_is_link_up(netif))
    {
        netif_set_link_down(netif);
        netif_set_down(netif);
        if (s_eth_started)
        {
            HAL_ETH_Stop(&heth);
            s_eth_started = false;
        }
    }
}

// Called by HAL to get a buffer for each incoming DMA descriptor.
void HAL_ETH_RxAllocateCallback(uint8_t** buff)
{
    *buff = RxBuff[rx_alloc_index % ETH_RX_DESC_CNT];
    ++rx_alloc_index;
}

// Called by HAL to build the pbuf chain from received DMA buffers.
void HAL_ETH_RxLinkCallback(
    void**    pStart,
    void**    pEnd,
    uint8_t*  buff,
    uint16_t  Length)
{
    auto** ppStart = reinterpret_cast<struct pbuf**>(pStart);
    auto** ppEnd   = reinterpret_cast<struct pbuf**>(pEnd);

    struct pbuf* const p = pbuf_alloc(PBUF_RAW,
                                      static_cast<uint16_t>(Length),
                                      PBUF_POOL);
    if (p == nullptr) { return; }

    pbuf_take(p, buff, Length);
    p->next    = nullptr;
    p->len     = Length;
    p->tot_len = Length;

    if (*ppStart == nullptr)
    {
        *ppStart = p;
        *ppEnd   = p;
    }
    else
    {
        (*ppEnd)->next = p;
        *ppEnd         = p;
        // Update tot_len for every node except the new tail (p), which already
        // has tot_len == Length set above.
        for (struct pbuf* q = *ppStart; q != p; q = q->next)
        {
            q->tot_len = static_cast<uint16_t>(q->tot_len + Length);
        }
    }
}

// Called by HAL after TX DMA completes — free the pbuf ref'd in low_level_output.
void HAL_ETH_TxFreeCallback(uint32_t* buff)
{
    pbuf_free(reinterpret_cast<struct pbuf*>(buff));
}

}  /* extern "C" */
