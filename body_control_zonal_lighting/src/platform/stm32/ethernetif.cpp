// LwIP netif glue for STM32H7 ETH peripheral (RMII, polling mode).
//
// Descriptor placement: .RxDecripSection / .TxDecripSection / .RxArraySection
// are placed in RAM_D2 by the linker script so the ETH DMA engine can
// reach them regardless of bus-matrix configuration.

#include <cstring>

#include "stm32h7xx_hal.h"

#include "lwip/err.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "netif/etharp.h"

extern ETH_HandleTypeDef heth;

namespace
{

constexpr std::uint8_t kMacAddr0 {0x02U};
constexpr std::uint8_t kMacAddr1 {0x00U};
constexpr std::uint8_t kMacAddr2 {0x00U};
constexpr std::uint8_t kMacAddr3 {0x00U};
constexpr std::uint8_t kMacAddr4 {0x00U};
constexpr std::uint8_t kMacAddr5 {0x01U};

ETH_DMADescTypeDef DMARxDscrTab[ETH_RX_DESC_CNT]
    __attribute__((section(".RxDecripSection"), aligned(4)));

ETH_DMADescTypeDef DMATxDscrTab[ETH_TX_DESC_CNT]
    __attribute__((section(".TxDecripSection"), aligned(4)));

std::uint8_t Rx_Buff[ETH_RX_DESC_CNT][ETH_MAX_PACKET_SIZE]
    __attribute__((section(".RxArraySection"), aligned(4)));

ETH_TxPacketConfig TxConfig {};

// ---- low_level_init --------------------------------------------------------

void low_level_init(struct netif* const netif)
{
    netif->hwaddr_len = ETH_HWADDR_LEN;
    netif->hwaddr[0]  = kMacAddr0;
    netif->hwaddr[1]  = kMacAddr1;
    netif->hwaddr[2]  = kMacAddr2;
    netif->hwaddr[3]  = kMacAddr3;
    netif->hwaddr[4]  = kMacAddr4;
    netif->hwaddr[5]  = kMacAddr5;

    netif->mtu = 1500U;
    netif->flags = static_cast<std::uint8_t>(
        NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP);

    // Provide descriptors and Rx buffers to the HAL.
    std::memset(DMATxDscrTab, 0, sizeof(DMATxDscrTab));
    std::memset(DMARxDscrTab, 0, sizeof(DMARxDscrTab));

    for (std::uint32_t i = 0U; i < ETH_RX_DESC_CNT; ++i)
    {
        HAL_ETH_DescAssignMemory(
            &heth,
            i,
            Rx_Buff[i],
            nullptr);
    }

    // Minimal TX config — no checksum offload in phase 6.
    std::memset(&TxConfig, 0, sizeof(TxConfig));
    TxConfig.Attributes  = ETH_TX_PACKETS_FEATURES_CRCPAD;
    TxConfig.CRCPadCtrl  = ETH_CRC_PAD_INSERT;

    HAL_ETH_Start(&heth);
}

// ---- low_level_output ------------------------------------------------------

err_t low_level_output(struct netif* /*netif*/, struct pbuf* const p)
{
    std::uint32_t i = 0U;
    struct pbuf*  q = p;
    err_t         ret = ERR_OK;

    ETH_BufferTypeDef tx_buffers[ETH_TX_DESC_CNT];
    std::memset(tx_buffers, 0, sizeof(tx_buffers));

    while (q != nullptr)
    {
        if (i >= ETH_TX_DESC_CNT) { return ERR_BUF; }

        tx_buffers[i].buffer = static_cast<std::uint8_t*>(q->payload);
        tx_buffers[i].len    = q->len;
        if (q->next != nullptr)
        {
            tx_buffers[i].next = &tx_buffers[i + 1U];
        }
        else
        {
            tx_buffers[i].next = nullptr;
        }
        ++i;
        q = q->next;
    }

    TxConfig.Length    = p->tot_len;
    TxConfig.TxBuffer  = tx_buffers;
    TxConfig.pData     = p;

    pbuf_ref(p);

    if (HAL_ETH_Transmit(&heth, &TxConfig, 100U) != HAL_OK)
    {
        pbuf_free(p);
        ret = ERR_IF;
    }

    return ret;
}

}  // namespace

// ---- Public API ------------------------------------------------------------

extern "C"
{

err_t ethernetif_init(struct netif* const netif)
{
    netif->name[0] = 'e';
    netif->name[1] = '0';
    netif->output      = etharp_output;
    netif->linkoutput  = low_level_output;

    low_level_init(netif);
    return ERR_OK;
}

void ethernetif_input(struct netif* const netif)
{
    ETH_BufferTypeDef rx_buffer {};
    std::uint32_t     frame_length = 0U;

    if (HAL_ETH_GetRxDataBuffer(&heth, &rx_buffer) != HAL_OK) { return; }
    if (HAL_ETH_GetRxDataLength(&heth, &frame_length) != HAL_OK)
    {
        HAL_ETH_BuildRxDescriptors(&heth);
        return;
    }

    struct pbuf* const p = pbuf_alloc(
        PBUF_RAW,
        static_cast<std::uint16_t>(frame_length),
        PBUF_POOL);

    if (p != nullptr)
    {
        pbuf_take(p, rx_buffer.buffer, static_cast<std::uint16_t>(frame_length));
        if (netif->input(p, netif) != ERR_OK)
        {
            pbuf_free(p);
        }
    }

    HAL_ETH_BuildRxDescriptors(&heth);
}

// Called by HAL when TX DMA completes — free the pbuf we ref'd in output.
void HAL_ETH_TxFreeCallback(std::uint32_t* buff)
{
    if (buff == nullptr) { return; }
    pbuf_free(reinterpret_cast<struct pbuf*>(buff));
}

}  /* extern "C" */
