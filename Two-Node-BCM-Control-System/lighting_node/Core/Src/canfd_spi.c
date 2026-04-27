#include "canfd_spi.h"
#include "spi.h"

#define MCP2517_CMD_RESET   0x00U
#define MCP2517_CMD_READ    0x03U
#define MCP2517_CMD_WRITE   0x02U

void CANFD_SPI_Select(void)
{
    HAL_GPIO_WritePin(CAN_CS_GPIO_Port, CAN_CS_Pin, GPIO_PIN_RESET);
}

void CANFD_SPI_Deselect(void)
{
    HAL_GPIO_WritePin(CAN_CS_GPIO_Port, CAN_CS_Pin, GPIO_PIN_SET);
}

HAL_StatusTypeDef CANFD_SPI_Reset(void)
{
    uint8_t tx[2] = {MCP2517_CMD_RESET, 0x00U};
    HAL_StatusTypeDef status;

    CANFD_SPI_Select();
    status = HAL_SPI_Transmit(&hspi1, tx, 2, HAL_MAX_DELAY);
    CANFD_SPI_Deselect();

    HAL_Delay(5);

    return status;
}

HAL_StatusTypeDef CANFD_SPI_ReadWord(uint16_t address, uint32_t *value)
{
    uint8_t tx[6];
    uint8_t rx[6] = {0};
    HAL_StatusTypeDef status;

    tx[0] = (uint8_t)((MCP2517_CMD_READ << 4) | ((address >> 8) & 0x0FU));
    tx[1] = (uint8_t)(address & 0xFFU);
    tx[2] = 0x00U;
    tx[3] = 0x00U;
    tx[4] = 0x00U;
    tx[5] = 0x00U;

    CANFD_SPI_Select();
    status = HAL_SPI_TransmitReceive(&hspi1, tx, rx, 6, HAL_MAX_DELAY);
    CANFD_SPI_Deselect();

    if (status != HAL_OK)
    {
        return status;
    }

    *value = ((uint32_t)rx[2]) |
             ((uint32_t)rx[3] << 8) |
             ((uint32_t)rx[4] << 16) |
             ((uint32_t)rx[5] << 24);

    return HAL_OK;
}

HAL_StatusTypeDef CANFD_SPI_WriteWord(uint16_t address, uint32_t value)
{
    uint8_t tx[6];
    HAL_StatusTypeDef status;

    tx[0] = (uint8_t)((MCP2517_CMD_WRITE << 4) | ((address >> 8) & 0x0FU));
    tx[1] = (uint8_t)(address & 0xFFU);
    tx[2] = (uint8_t)(value & 0xFFU);
    tx[3] = (uint8_t)((value >> 8) & 0xFFU);
    tx[4] = (uint8_t)((value >> 16) & 0xFFU);
    tx[5] = (uint8_t)((value >> 24) & 0xFFU);

    CANFD_SPI_Select();
    status = HAL_SPI_Transmit(&hspi1, tx, 6, HAL_MAX_DELAY);
    CANFD_SPI_Deselect();

    return status;
}


HAL_StatusTypeDef CANFD_SPI_ReadBytes(uint16_t address, uint8_t *data, uint16_t len)
{
    HAL_StatusTypeDef status;
    uint16_t total_len;
    uint16_t i;

    if ((data == 0) || (len == 0))
    {
        return HAL_ERROR;
    }

    total_len = (uint16_t)(len + 2U);

    if (total_len > 128U)
    {
        return HAL_ERROR;
    }

    uint8_t tx[128] = {0};
    uint8_t rx[128] = {0};

    tx[0] = (uint8_t)((MCP2517_CMD_READ << 4) | ((address >> 8) & 0x0FU));
    tx[1] = (uint8_t)(address & 0xFFU);

    CANFD_SPI_Select();
    status = HAL_SPI_TransmitReceive(&hspi1, tx, rx, total_len, HAL_MAX_DELAY);
    CANFD_SPI_Deselect();

    if (status != HAL_OK)
    {
        return status;
    }

    for (i = 0; i < len; i++)
    {
        data[i] = rx[i + 2U];
    }

    return HAL_OK;
}

HAL_StatusTypeDef CANFD_SPI_WriteBytes(uint16_t address, const uint8_t *data, uint16_t len)
{
    HAL_StatusTypeDef status;
    uint16_t total_len;
    uint16_t i;

    if ((data == 0) || (len == 0))
    {
        return HAL_ERROR;
    }

    total_len = (uint16_t)(len + 2U);

    if (total_len > 128U)
    {
        return HAL_ERROR;
    }

    uint8_t tx[128] = {0};

    tx[0] = (uint8_t)((MCP2517_CMD_WRITE << 4) | ((address >> 8) & 0x0FU));
    tx[1] = (uint8_t)(address & 0xFFU);

    for (i = 0; i < len; i++)
    {
        tx[i + 2U] = data[i];
    }

    CANFD_SPI_Select();
    status = HAL_SPI_Transmit(&hspi1, tx, total_len, HAL_MAX_DELAY);
    CANFD_SPI_Deselect();

    return status;
}
