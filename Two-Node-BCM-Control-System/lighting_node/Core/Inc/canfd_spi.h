#ifndef INC_CANFD_SPI_H_
#define INC_CANFD_SPI_H_

#include "main.h"

HAL_StatusTypeDef CANFD_SPI_Reset(void);
HAL_StatusTypeDef CANFD_SPI_ReadWord(uint16_t address, uint32_t *value);
HAL_StatusTypeDef CANFD_SPI_WriteWord(uint16_t address, uint32_t value);

void CANFD_SPI_Select(void);
void CANFD_SPI_Deselect(void);

HAL_StatusTypeDef CANFD_SPI_ReadBytes(uint16_t address, uint8_t *data, uint16_t len);
HAL_StatusTypeDef CANFD_SPI_WriteBytes(uint16_t address, const uint8_t *data, uint16_t len);
#endif /* INC_CANFD_SPI_H_ */
