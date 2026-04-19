// LwIP netif glue for STM32H7 ETH peripheral (RMII, polling NO_SYS mode).
//
// RX path:  HAL calls RxAllocateCallback to get a DMA buffer, then calls
//           RxLinkCallback with the received bytes.  ethernetif_input() polls
//           HAL_ETH_ReadData() which drives both callbacks and returns a pbuf
//           chain ready for netif->input().
//
// TX path:  low_level_output() builds an ETH_BufferTypeDef scatter list from
//           the pbuf chain and calls HAL_ETH_Transmit().  The HAL calls
//           HAL_ETH_TxFreeCallback() after DMA completes.
//
// ETH DMA descriptor tables and static RX buffers are placed in RAM_D2 via
// the linker-script sections .RxDecripSection / .TxDecripSection / .RxArraySection
// so the ETH DMA engine can reach them on the D2 AXI bus.

#include <cstring>

#include "stm32h7xx_hal.h"

#include "lwip/err.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "netif/etharp.h"

// ---- ETH handle (shared with stm32h7xx_it.cpp and main.cpp) ----------------

ETH_HandleTypeDef heth;

// ---- DMA descriptor tables and static RX buffers in RAM_D2 -----------------

namespace
{

constexpr uint8_t kMacAddr0 {0x02U};
constexpr uint8_t kMacAddr1 {0x00U};
constexpr uint8_t kMacAddr2 {0x00U};
constexpr uint8_t kMacAddr3 {0x00U};
constexpr uint8_t kMacAddr4 {0x00U};
constexpr uint8_t kMacAddr5 {0x01U};

ETH_DMADescTypeDef DMARxDscrTab[ETH_RX_DESC_CNT]
    __attribute__((section(".RxDecripSection"), aligned(4)));

ETH_DMADescTypeDef DMATxDscrTab[ETH_TX_DESC_CNT]
    __attribute__((section(".TxDecripSection"), aligned(4)));

// One static buffer per RX descriptor.  ETH DMA writes directly into these.
uint8_t RxBuff[ETH_RX_DESC_CNT][ETH_MAX_PACKET_SIZE]
    __attribute__((section(".RxArraySection"), aligned(4)));

// Which RxBuff slot to offer to the HAL next.
uint32_t rx_alloc_index {0U};

// ---- low_level_init --------------------------------------------------------

void low_level_init(struct netif* const netif)
{
    static uint8_t mac_addr[6U] = {
        kMacAddr0, kMacAddr1, kMacAddr2,
        kMacAddr3, kMacAddr4, kMacAddr5
    };

    heth.Instance           = ETH;
    heth.Init.MACAddr       = mac_addr;
    heth.Init.MediaInterface= HAL_ETH_RMII_MODE;
    heth.Init.TxDesc        = DMATxDscrTab;
    heth.Init.RxDesc        = DMARxDscrTab;
    heth.Init.RxBuffLen     = ETH_MAX_PACKET_SIZE;

    HAL_ETH_Init(&heth);

    netif->hwaddr_len = ETH_HWADDR_LEN;
    netif->hwaddr[0]  = kMacAddr0;
    netif->hwaddr[1]  = kMacAddr1;
    netif->hwaddr[2]  = kMacAddr2;
    netif->hwaddr[3]  = kMacAddr3;
    netif->hwaddr[4]  = kMacAddr4;
    netif->hwaddr[5]  = kMacAddr5;

    netif->mtu   = 1500U;
    netif->flags = static_cast<uint8_t>(
        NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP);

    HAL_ETH_Start(&heth);
}

// ---- low_level_output ------------------------------------------------------

err_t low_level_output(struct netif* /*netif*/, struct pbuf* const p)
{
    ETH_BufferTypeDef    tx_buffers[ETH_TX_DESC_CNT];
    ETH_TxPacketConfigTypeDef tx_cfg {};
    uint32_t        i {0U};

    std::memset(tx_buffers, 0, sizeof(tx_buffers));

    for (struct pbuf* q = p; q != nullptr; q = q->next)
    {
        if (i >= ETH_TX_DESC_CNT) { return ERR_BUF; }

        tx_buffers[i].buffer = static_cast<uint8_t*>(q->payload);
        tx_buffers[i].len    = q->len;
        if (i > 0U) { tx_buffers[i - 1U].next = &tx_buffers[i]; }
        ++i;
    }
    if (i > 0U) { tx_buffers[i - 1U].next = nullptr; }

    tx_cfg.Attributes  = ETH_TX_PACKETS_FEATURES_CRCPAD;
    tx_cfg.CRCPadCtrl  = ETH_CRC_PAD_INSERT;
    tx_cfg.Length      = p->tot_len;
    tx_cfg.TxBuffer    = tx_buffers;
    tx_cfg.pData       = p;

    pbuf_ref(p);

    if (HAL_ETH_Transmit(&heth, &tx_cfg, 100U) != HAL_OK)
    {
        pbuf_free(p);
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
        *ppEnd = p;
        for (struct pbuf* q = *ppStart; q != nullptr; q = q->next)
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
