#include "mcp2517fd_can.h"
#include "canfd_spi.h"
#include "can_test.h"

HAL_StatusTypeDef MCP2517FD_CAN_ReadC1CON(uint32_t *value)
{
    return CANFD_SPI_ReadWord(MCP2517FD_REG_C1CON, value);
}

HAL_StatusTypeDef MCP2517FD_CAN_GetOpMode(uint8_t *mode)
{
    uint32_t c1con = 0;
    HAL_StatusTypeDef status;

    status = MCP2517FD_CAN_ReadC1CON(&c1con);
    if (status != HAL_OK)
    {
        return status;
    }

    *mode = (uint8_t)((c1con & MCP2517FD_OPMOD_MASK) >> MCP2517FD_OPMOD_SHIFT);
    return HAL_OK;
}

HAL_StatusTypeDef MCP2517FD_CAN_RequestMode(uint8_t reqop, uint8_t *final_mode)
{
    uint32_t c1con = 0;
    HAL_StatusTypeDef status;
    uint32_t timeout = 1000U;
    uint8_t mode_now = 0;

    status = MCP2517FD_CAN_ReadC1CON(&c1con);
    if (status != HAL_OK)
    {
        return status;
    }

    c1con &= ~MCP2517FD_REQOP_MASK;
    c1con |= (((uint32_t)(reqop & 0x7U)) << MCP2517FD_REQOP_SHIFT);

    status = CANFD_SPI_WriteWord(MCP2517FD_REG_C1CON, c1con);
    if (status != HAL_OK)
    {
        return status;
    }

    do
    {
        status = MCP2517FD_CAN_GetOpMode(&mode_now);
        if (status != HAL_OK)
        {
            return status;
        }

        if (mode_now == reqop)
        {
            *final_mode = mode_now;
            return HAL_OK;
        }

        timeout--;
    } while (timeout > 0U);

    *final_mode = mode_now;
    return HAL_TIMEOUT;
}

HAL_StatusTypeDef MCP2517FD_CAN_SetNominalBitTiming_500k_20MHz(void)
{
    /* 20 MHz CAN clock, 500 kbps nominal */
    return CANFD_SPI_WriteWord(MCP2517FD_REG_C1NBTCFG, 0x001E0707U);
}

HAL_StatusTypeDef MCP2517FD_CAN_SetDataBitTiming_2M_20MHz(void)
{
    /*
     * 20 MHz CAN clock, 2 Mbps data phase
     * Sync=1, DTSEG1=7, DTSEG2=2, DSJW=2
     * Encoded as value-1 fields
     */
    return CANFD_SPI_WriteWord(MCP2517FD_REG_C1DBTCFG, 0x00060101U);
}

HAL_StatusTypeDef MCP2517FD_CAN_EnableTdcAuto(void)
{
    /* TDCMOD = Auto */
    return CANFD_SPI_WriteWord(MCP2517FD_REG_C1TDC, 0x00020000U);
}

HAL_StatusTypeDef MCP2517FD_CAN_EnableBRS(void)
{
    HAL_StatusTypeDef status;
    uint32_t c1con = 0;

    status = MCP2517FD_CAN_ReadC1CON(&c1con);
    if (status != HAL_OK)
    {
        return status;
    }

    /* Clear BRSDIS (bit 12) */
    c1con &= ~(1UL << 12);

    return CANFD_SPI_WriteWord(MCP2517FD_REG_C1CON, c1con);
}

HAL_StatusTypeDef MCP2517FD_CAN_RequestNormalCAN20(uint8_t *final_mode)
{
    return MCP2517FD_CAN_RequestMode(MCP2517FD_MODE_NORMAL_CAN20, final_mode);
}

HAL_StatusTypeDef MCP2517FD_CAN_RequestNormalFD(uint8_t *final_mode)
{
    return MCP2517FD_CAN_RequestMode(MCP2517FD_MODE_NORMAL_FD, final_mode);
}

HAL_StatusTypeDef MCP2517FD_CAN_RequestIntLoopback(uint8_t *final_mode)
{
    return MCP2517FD_CAN_RequestMode(MCP2517FD_MODE_INT_LOOPBACK, final_mode);
}

HAL_StatusTypeDef MCP2517FD_CAN_ConfigureTxFifo1_8B(void)
{
    return CANFD_SPI_WriteWord(MCP2517FD_REG_C1FIFOCON1, 0x00000080U);
}

HAL_StatusTypeDef MCP2517FD_CAN_ConfigureTxFifo1_16B(void)
{
    /* PLSIZE=010 => 16 bytes, TXEN=1 */
    return CANFD_SPI_WriteWord(MCP2517FD_REG_C1FIFOCON1, 0x40000080U);
}

HAL_StatusTypeDef MCP2517FD_CAN_ConfigureRxFifo1_8B_TS(void)
{
    return CANFD_SPI_WriteWord(MCP2517FD_REG_C1FIFOCON1, 0x00000020U);
}

HAL_StatusTypeDef MCP2517FD_CAN_ConfigureRxFifo2_8B_TS(void)
{
    return CANFD_SPI_WriteWord(MCP2517FD_REG_C1FIFOCON2, 0x00000020U);
}

HAL_StatusTypeDef MCP2517FD_CAN_ConfigureRxFifo2_16B_TS(void)
{
    /* PLSIZE=010 => 16 bytes, RXTSEN=1 */
    return CANFD_SPI_WriteWord(MCP2517FD_REG_C1FIFOCON2, 0x40000020U);
}

HAL_StatusTypeDef MCP2517FD_CAN_ConfigureFilter0_Std11_ToFifo1(uint16_t sid)
{
    HAL_StatusTypeDef status;

    status = CANFD_SPI_WriteWord(MCP2517FD_REG_C1FLTOBJ0, (uint32_t)(sid & 0x7FFU));
    if (status != HAL_OK)
    {
        return status;
    }

    status = CANFD_SPI_WriteWord(MCP2517FD_REG_C1MASK0, 0x400007FFU);
    if (status != HAL_OK)
    {
        return status;
    }

    return CANFD_SPI_WriteWord(MCP2517FD_REG_C1FLTCON0, 0x00000081U);
}

HAL_StatusTypeDef MCP2517FD_CAN_ConfigureFilter0_AcceptAll_ToFifo1(void)
{
    HAL_StatusTypeDef status;

    status = CANFD_SPI_WriteWord(MCP2517FD_REG_C1FLTOBJ0, 0x00000000U);
    if (status != HAL_OK)
    {
        return status;
    }

    status = CANFD_SPI_WriteWord(MCP2517FD_REG_C1MASK0, 0x00000000U);
    if (status != HAL_OK)
    {
        return status;
    }

    return CANFD_SPI_WriteWord(MCP2517FD_REG_C1FLTCON0, 0x00000081U);
}

HAL_StatusTypeDef MCP2517FD_CAN_ConfigureFilter0_Std11_ToFifo2(uint16_t sid)
{
    HAL_StatusTypeDef status;

    status = CANFD_SPI_WriteWord(MCP2517FD_REG_C1FLTOBJ0, (uint32_t)(sid & 0x7FFU));
    if (status != HAL_OK)
    {
        return status;
    }

    status = CANFD_SPI_WriteWord(MCP2517FD_REG_C1MASK0, 0x400007FFU);
    if (status != HAL_OK)
    {
        return status;
    }

    return CANFD_SPI_WriteWord(MCP2517FD_REG_C1FLTCON0, 0x00000082U);
}

HAL_StatusTypeDef MCP2517FD_CAN_ConfigureFilter0_AcceptAll_ToFifo2(void)
{
    HAL_StatusTypeDef status;

    status = CANFD_SPI_WriteWord(MCP2517FD_REG_C1FLTOBJ0, 0x00000000U);
    if (status != HAL_OK)
    {
        return status;
    }

    status = CANFD_SPI_WriteWord(MCP2517FD_REG_C1MASK0, 0x00000000U);
    if (status != HAL_OK)
    {
        return status;
    }

    return CANFD_SPI_WriteWord(MCP2517FD_REG_C1FLTCON0, 0x00000082U);
}

HAL_StatusTypeDef MCP2517FD_CAN_ReadFifo1CON(uint32_t *value)
{
    return CANFD_SPI_ReadWord(MCP2517FD_REG_C1FIFOCON1, value);
}

HAL_StatusTypeDef MCP2517FD_CAN_ReadFifo1STA(uint32_t *value)
{
    return CANFD_SPI_ReadWord(MCP2517FD_REG_C1FIFOSTA1, value);
}

HAL_StatusTypeDef MCP2517FD_CAN_ReadFifo1UA(uint32_t *value)
{
    return CANFD_SPI_ReadWord(MCP2517FD_REG_C1FIFOUA1, value);
}

HAL_StatusTypeDef MCP2517FD_CAN_ReadFifo2CON(uint32_t *value)
{
    return CANFD_SPI_ReadWord(MCP2517FD_REG_C1FIFOCON2, value);
}

HAL_StatusTypeDef MCP2517FD_CAN_ReadFifo2STA(uint32_t *value)
{
    return CANFD_SPI_ReadWord(MCP2517FD_REG_C1FIFOSTA2, value);
}

HAL_StatusTypeDef MCP2517FD_CAN_ReadFifo2UA(uint32_t *value)
{
    return CANFD_SPI_ReadWord(MCP2517FD_REG_C1FIFOUA2, value);
}

HAL_StatusTypeDef MCP2517FD_CAN_TxFifo1SendStd8(uint16_t sid, const uint8_t data[8])
{
    HAL_StatusTypeDef status;
    uint32_t ua = 0;
    uint32_t fifo1con = 0;
    uint16_t ram_addr = 0;
    uint8_t msg[16] = {0};

    status = MCP2517FD_CAN_ReadFifo1UA(&ua);
    if (status != HAL_OK)
    {
        return status;
    }

    ram_addr = (uint16_t)(MCP2517FD_RAM_BASE + (ua & 0x0FFFU));

    msg[0] = (uint8_t)(sid & 0xFFU);
    msg[1] = (uint8_t)((sid >> 8) & 0x07U);
    msg[2] = 0x00U;
    msg[3] = 0x00U;

    msg[4] = 0x08U;
    msg[5] = 0x00U;
    msg[6] = 0x00U;
    msg[7] = 0x00U;

    msg[8]  = data[0];
    msg[9]  = data[1];
    msg[10] = data[2];
    msg[11] = data[3];
    msg[12] = data[4];
    msg[13] = data[5];
    msg[14] = data[6];
    msg[15] = data[7];

    status = CANFD_SPI_WriteBytes(ram_addr, msg, 16);
    if (status != HAL_OK)
    {
        return status;
    }

    status = MCP2517FD_CAN_ReadFifo1CON(&fifo1con);
    if (status != HAL_OK)
    {
        return status;
    }

    return CANFD_SPI_WriteWord(MCP2517FD_REG_C1FIFOCON1,
                               fifo1con | (1U << 8) | (1U << 9));
}

HAL_StatusTypeDef MCP2517FD_CAN_TxFifo1SendFd16(uint16_t sid,
                                                const uint8_t data[16],
                                                uint8_t brs)
{
    HAL_StatusTypeDef status;
    uint32_t ua = 0;
    uint32_t fifo1con = 0;
    uint16_t ram_addr = 0;
    uint8_t msg[24] = {0};

    status = MCP2517FD_CAN_ReadFifo1UA(&ua);
    if (status != HAL_OK)
    {
        return status;
    }

    ram_addr = (uint16_t)(MCP2517FD_RAM_BASE + (ua & 0x0FFFU));

    msg[0] = (uint8_t)(sid & 0xFFU);
    msg[1] = (uint8_t)((sid >> 8) & 0x07U);
    msg[2] = 0x00U;
    msg[3] = 0x00U;

    /* FDF=1, BRS selectable, DLC=10 => 16 bytes */
    msg[4] = (uint8_t)(0x80U | ((brs ? 1U : 0U) << 6) | CAN_TEST_FD_DLC_16);
    msg[5] = 0x00U;
    msg[6] = 0x00U;
    msg[7] = 0x00U;

    msg[8]  = data[0];
    msg[9]  = data[1];
    msg[10] = data[2];
    msg[11] = data[3];
    msg[12] = data[4];
    msg[13] = data[5];
    msg[14] = data[6];
    msg[15] = data[7];
    msg[16] = data[8];
    msg[17] = data[9];
    msg[18] = data[10];
    msg[19] = data[11];
    msg[20] = data[12];
    msg[21] = data[13];
    msg[22] = data[14];
    msg[23] = data[15];

    status = CANFD_SPI_WriteBytes(ram_addr, msg, 24);
    if (status != HAL_OK)
    {
        return status;
    }

    status = MCP2517FD_CAN_ReadFifo1CON(&fifo1con);
    if (status != HAL_OK)
    {
        return status;
    }

    return CANFD_SPI_WriteWord(MCP2517FD_REG_C1FIFOCON1,
                               fifo1con | (1U << 8) | (1U << 9));
}

HAL_StatusTypeDef MCP2517FD_CAN_RxFifo1PollStd8(uint16_t *sid,
                                                uint8_t data[8],
                                                uint8_t *dlc,
                                                uint8_t *got_frame)
{
    HAL_StatusTypeDef status;
    uint32_t sta = 0;
    uint32_t ua = 0;
    uint32_t fifo1con = 0;
    uint16_t ram_addr = 0;
    uint8_t msg[20] = {0};

    *got_frame = 0U;

    status = MCP2517FD_CAN_ReadFifo1STA(&sta);
    if (status != HAL_OK)
    {
        return status;
    }

    if ((sta & 0x01U) == 0U)
    {
        return HAL_OK;
    }

    status = MCP2517FD_CAN_ReadFifo1UA(&ua);
    if (status != HAL_OK)
    {
        return status;
    }

    ram_addr = (uint16_t)(MCP2517FD_RAM_BASE + (ua & 0x0FFFU));

    status = CANFD_SPI_ReadBytes(ram_addr, msg, 20);
    if (status != HAL_OK)
    {
        return status;
    }

    *sid = (uint16_t)((((uint16_t)msg[1] & 0x07U) << 8) | msg[0]);
    *dlc = (uint8_t)(msg[4] & 0x0FU);

    data[0] = msg[12];
    data[1] = msg[13];
    data[2] = msg[14];
    data[3] = msg[15];
    data[4] = msg[16];
    data[5] = msg[17];
    data[6] = msg[18];
    data[7] = msg[19];

    status = MCP2517FD_CAN_ReadFifo1CON(&fifo1con);
    if (status != HAL_OK)
    {
        return status;
    }

    status = CANFD_SPI_WriteWord(MCP2517FD_REG_C1FIFOCON1, fifo1con | (1U << 8));
    if (status != HAL_OK)
    {
        return status;
    }

    *got_frame = 1U;
    return HAL_OK;
}

HAL_StatusTypeDef MCP2517FD_CAN_RxFifo2PollStd8(uint16_t *sid,
                                                uint8_t data[8],
                                                uint8_t *dlc,
                                                uint8_t *got_frame)
{
    HAL_StatusTypeDef status;
    uint32_t sta = 0;
    uint32_t ua = 0;
    uint32_t fifo2con = 0;
    uint16_t ram_addr = 0;
    uint8_t msg[20] = {0};

    *got_frame = 0U;

    status = MCP2517FD_CAN_ReadFifo2STA(&sta);
    if (status != HAL_OK)
    {
        return status;
    }

    if ((sta & 0x01U) == 0U)
    {
        return HAL_OK;
    }

    status = MCP2517FD_CAN_ReadFifo2UA(&ua);
    if (status != HAL_OK)
    {
        return status;
    }

    ram_addr = (uint16_t)(MCP2517FD_RAM_BASE + (ua & 0x0FFFU));

    status = CANFD_SPI_ReadBytes(ram_addr, msg, 20);
    if (status != HAL_OK)
    {
        return status;
    }

    *sid = (uint16_t)((((uint16_t)msg[1] & 0x07U) << 8) | msg[0]);
    *dlc = (uint8_t)(msg[4] & 0x0FU);

    data[0] = msg[12];
    data[1] = msg[13];
    data[2] = msg[14];
    data[3] = msg[15];
    data[4] = msg[16];
    data[5] = msg[17];
    data[6] = msg[18];
    data[7] = msg[19];

    status = MCP2517FD_CAN_ReadFifo2CON(&fifo2con);
    if (status != HAL_OK)
    {
        return status;
    }

    status = CANFD_SPI_WriteWord(MCP2517FD_REG_C1FIFOCON2, fifo2con | (1U << 8));
    if (status != HAL_OK)
    {
        return status;
    }

    *got_frame = 1U;
    return HAL_OK;
}

HAL_StatusTypeDef MCP2517FD_CAN_RxFifo2PollFd16(uint16_t *sid,
                                                uint8_t data[16],
                                                uint8_t *dlc,
                                                uint8_t *fdf,
                                                uint8_t *brs,
                                                uint8_t *got_frame)
{
    HAL_StatusTypeDef status;
    uint32_t sta = 0;
    uint32_t ua = 0;
    uint32_t fifo2con = 0;
    uint16_t ram_addr = 0;
    uint8_t msg[28] = {0};

    *got_frame = 0U;

    status = MCP2517FD_CAN_ReadFifo2STA(&sta);
    if (status != HAL_OK)
    {
        return status;
    }

    if ((sta & 0x01U) == 0U)
    {
        return HAL_OK;
    }

    status = MCP2517FD_CAN_ReadFifo2UA(&ua);
    if (status != HAL_OK)
    {
        return status;
    }

    ram_addr = (uint16_t)(MCP2517FD_RAM_BASE + (ua & 0x0FFFU));

    status = CANFD_SPI_ReadBytes(ram_addr, msg, 28);
    if (status != HAL_OK)
    {
        return status;
    }

    *sid = (uint16_t)((((uint16_t)msg[1] & 0x07U) << 8) | msg[0]);
    *dlc = (uint8_t)(msg[4] & 0x0FU);
    *fdf = (uint8_t)((msg[4] >> 7) & 0x01U);
    *brs = (uint8_t)((msg[4] >> 6) & 0x01U);

    data[0]  = msg[12];
    data[1]  = msg[13];
    data[2]  = msg[14];
    data[3]  = msg[15];
    data[4]  = msg[16];
    data[5]  = msg[17];
    data[6]  = msg[18];
    data[7]  = msg[19];
    data[8]  = msg[20];
    data[9]  = msg[21];
    data[10] = msg[22];
    data[11] = msg[23];
    data[12] = msg[24];
    data[13] = msg[25];
    data[14] = msg[26];
    data[15] = msg[27];

    status = MCP2517FD_CAN_ReadFifo2CON(&fifo2con);
    if (status != HAL_OK)
    {
        return status;
    }

    status = CANFD_SPI_WriteWord(MCP2517FD_REG_C1FIFOCON2, fifo2con | (1U << 8));
    if (status != HAL_OK)
    {
        return status;
    }

    *got_frame = 1U;
    return HAL_OK;
}

HAL_StatusTypeDef MCP2517FD_CAN_RxFifo1DumpRaw20(uint8_t raw[20],
                                                 uint32_t *ua,
                                                 uint32_t *sta,
                                                 uint32_t *con)
{
    HAL_StatusTypeDef status;
    uint16_t ram_addr = 0;

    status = MCP2517FD_CAN_ReadFifo1CON(con);
    if (status != HAL_OK)
    {
        return status;
    }

    status = MCP2517FD_CAN_ReadFifo1STA(sta);
    if (status != HAL_OK)
    {
        return status;
    }

    status = MCP2517FD_CAN_ReadFifo1UA(ua);
    if (status != HAL_OK)
    {
        return status;
    }

    ram_addr = (uint16_t)(MCP2517FD_RAM_BASE + ((*ua) & 0x0FFFU));

    return CANFD_SPI_ReadBytes(ram_addr, raw, 20);
}

HAL_StatusTypeDef MCP2517FD_CAN_RxFifo2DumpRaw20(uint8_t raw[20],
                                                 uint32_t *ua,
                                                 uint32_t *sta,
                                                 uint32_t *con)
{
    HAL_StatusTypeDef status;
    uint16_t ram_addr = 0;

    status = MCP2517FD_CAN_ReadFifo2CON(con);
    if (status != HAL_OK)
    {
        return status;
    }

    status = MCP2517FD_CAN_ReadFifo2STA(sta);
    if (status != HAL_OK)
    {
        return status;
    }

    status = MCP2517FD_CAN_ReadFifo2UA(ua);
    if (status != HAL_OK)
    {
        return status;
    }

    ram_addr = (uint16_t)(MCP2517FD_RAM_BASE + ((*ua) & 0x0FFFU));

    return CANFD_SPI_ReadBytes(ram_addr, raw, 20);
}
