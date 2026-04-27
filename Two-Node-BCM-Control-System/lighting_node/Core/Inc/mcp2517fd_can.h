#ifndef INC_MCP2517FD_CAN_H_
#define INC_MCP2517FD_CAN_H_

#include "main.h"

#define MCP2517FD_RAM_BASE          0x400U

#define MCP2517FD_REG_C1CON         0x000U
#define MCP2517FD_REG_C1NBTCFG      0x004U
#define MCP2517FD_REG_C1DBTCFG      0x008U
#define MCP2517FD_REG_C1TDC         0x00CU

#define MCP2517FD_REG_C1FIFOCON1    0x05CU
#define MCP2517FD_REG_C1FIFOSTA1    0x060U
#define MCP2517FD_REG_C1FIFOUA1     0x064U

#define MCP2517FD_REG_C1FIFOCON2    0x068U
#define MCP2517FD_REG_C1FIFOSTA2    0x06CU
#define MCP2517FD_REG_C1FIFOUA2     0x070U

#define MCP2517FD_REG_C1FLTCON0     0x1D0U
#define MCP2517FD_REG_C1FLTOBJ0     0x1F0U
#define MCP2517FD_REG_C1MASK0       0x1F4U

#define MCP2517FD_REG_OSC           0xE00U

#define MCP2517FD_REQOP_SHIFT       24U
#define MCP2517FD_REQOP_MASK        (0x7U << MCP2517FD_REQOP_SHIFT)

#define MCP2517FD_OPMOD_SHIFT       21U
#define MCP2517FD_OPMOD_MASK        (0x7U << MCP2517FD_OPMOD_SHIFT)

#define MCP2517FD_MODE_NORMAL_FD      0x0U
#define MCP2517FD_MODE_SLEEP          0x1U
#define MCP2517FD_MODE_INT_LOOPBACK   0x2U
#define MCP2517FD_MODE_LISTEN_ONLY    0x3U
#define MCP2517FD_MODE_CONFIG         0x4U
#define MCP2517FD_MODE_EXT_LOOPBACK   0x5U
#define MCP2517FD_MODE_NORMAL_CAN20   0x6U
#define MCP2517FD_MODE_RESTRICTED     0x7U

HAL_StatusTypeDef MCP2517FD_CAN_ReadC1CON(uint32_t *value);
HAL_StatusTypeDef MCP2517FD_CAN_GetOpMode(uint8_t *mode);
HAL_StatusTypeDef MCP2517FD_CAN_RequestMode(uint8_t reqop, uint8_t *final_mode);

HAL_StatusTypeDef MCP2517FD_CAN_SetNominalBitTiming_500k_20MHz(void);
HAL_StatusTypeDef MCP2517FD_CAN_SetDataBitTiming_2M_20MHz(void);
HAL_StatusTypeDef MCP2517FD_CAN_EnableTdcAuto(void);
HAL_StatusTypeDef MCP2517FD_CAN_EnableBRS(void);

HAL_StatusTypeDef MCP2517FD_CAN_RequestNormalCAN20(uint8_t *final_mode);
HAL_StatusTypeDef MCP2517FD_CAN_RequestNormalFD(uint8_t *final_mode);
HAL_StatusTypeDef MCP2517FD_CAN_RequestIntLoopback(uint8_t *final_mode);

HAL_StatusTypeDef MCP2517FD_CAN_ConfigureTxFifo1_8B(void);
HAL_StatusTypeDef MCP2517FD_CAN_ConfigureTxFifo1_16B(void);

HAL_StatusTypeDef MCP2517FD_CAN_ConfigureRxFifo1_8B_TS(void);
HAL_StatusTypeDef MCP2517FD_CAN_ConfigureRxFifo2_8B_TS(void);
HAL_StatusTypeDef MCP2517FD_CAN_ConfigureRxFifo2_16B_TS(void);

HAL_StatusTypeDef MCP2517FD_CAN_ConfigureFilter0_Std11_ToFifo1(uint16_t sid);
HAL_StatusTypeDef MCP2517FD_CAN_ConfigureFilter0_AcceptAll_ToFifo1(void);
HAL_StatusTypeDef MCP2517FD_CAN_ConfigureFilter0_Std11_ToFifo2(uint16_t sid);
HAL_StatusTypeDef MCP2517FD_CAN_ConfigureFilter0_AcceptAll_ToFifo2(void);

HAL_StatusTypeDef MCP2517FD_CAN_ReadFifo1CON(uint32_t *value);
HAL_StatusTypeDef MCP2517FD_CAN_ReadFifo1STA(uint32_t *value);
HAL_StatusTypeDef MCP2517FD_CAN_ReadFifo1UA(uint32_t *value);

HAL_StatusTypeDef MCP2517FD_CAN_ReadFifo2CON(uint32_t *value);
HAL_StatusTypeDef MCP2517FD_CAN_ReadFifo2STA(uint32_t *value);
HAL_StatusTypeDef MCP2517FD_CAN_ReadFifo2UA(uint32_t *value);

HAL_StatusTypeDef MCP2517FD_CAN_TxFifo1SendStd8(uint16_t sid, const uint8_t data[8]);
HAL_StatusTypeDef MCP2517FD_CAN_TxFifo1SendFd16(uint16_t sid,
                                                const uint8_t data[16],
                                                uint8_t brs);

HAL_StatusTypeDef MCP2517FD_CAN_RxFifo1PollStd8(uint16_t *sid,
                                                uint8_t data[8],
                                                uint8_t *dlc,
                                                uint8_t *got_frame);

HAL_StatusTypeDef MCP2517FD_CAN_RxFifo2PollStd8(uint16_t *sid,
                                                uint8_t data[8],
                                                uint8_t *dlc,
                                                uint8_t *got_frame);

HAL_StatusTypeDef MCP2517FD_CAN_RxFifo2PollFd16(uint16_t *sid,
                                                uint8_t data[16],
                                                uint8_t *dlc,
                                                uint8_t *fdf,
                                                uint8_t *brs,
                                                uint8_t *got_frame);

HAL_StatusTypeDef MCP2517FD_CAN_RxFifo1DumpRaw20(uint8_t raw[20],
                                                 uint32_t *ua,
                                                 uint32_t *sta,
                                                 uint32_t *con);

HAL_StatusTypeDef MCP2517FD_CAN_RxFifo2DumpRaw20(uint8_t raw[20],
                                                 uint32_t *ua,
                                                 uint32_t *sta,
                                                 uint32_t *con);

#endif /* INC_MCP2517FD_CAN_H_ */
