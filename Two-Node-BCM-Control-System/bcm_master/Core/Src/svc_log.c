#include "svc_log.h"
#include <stddef.h>

static UART_HandleTypeDef * gp_uart = NULL;

void SvcLog_Init(UART_HandleTypeDef * p_uart)
{
    gp_uart = p_uart;
}

void SvcLog_BufferInit(SvcLog_Buffer_t * p_buffer)
{
    uint16_t index;

    if (p_buffer != NULL)
    {
        p_buffer->length = 0U;

        for (index = 0U; index < SVC_LOG_BUFFER_SIZE; index++)
        {
            p_buffer->data[index] = '\0';
        }
    }
}

void SvcLog_AppendChar(SvcLog_Buffer_t * p_buffer, char value)
{
    if (p_buffer != NULL)
    {
        if (p_buffer->length < (SVC_LOG_BUFFER_SIZE - 1U))
        {
            p_buffer->data[p_buffer->length] = value;
            p_buffer->length++;
            p_buffer->data[p_buffer->length] = '\0';
        }
    }
}

void SvcLog_AppendText(SvcLog_Buffer_t * p_buffer, const char * p_text)
{
    uint16_t index;

    index = 0U;

    if ((p_buffer == NULL) || (p_text == NULL))
    {
        return;
    }

    while (p_text[index] != '\0')
    {
        SvcLog_AppendChar(p_buffer, p_text[index]);
        index++;
    }
}

void SvcLog_AppendU32(SvcLog_Buffer_t * p_buffer, uint32_t value)
{
    char digits[10];
    uint8_t count;
    uint32_t local_value;

    count = 0U;
    local_value = value;

    if (local_value == 0UL)
    {
        SvcLog_AppendChar(p_buffer, '0');
    }
    else
    {
        while ((local_value > 0UL) && (count < 10U))
        {
            digits[count] = (char)('0' + (char)(local_value % 10UL));
            local_value /= 10UL;
            count++;
        }

        while (count > 0U)
        {
            count--;
            SvcLog_AppendChar(p_buffer, digits[count]);
        }
    }
}

void SvcLog_AppendHex8(SvcLog_Buffer_t * p_buffer, uint8_t value)
{
    static const char hex_table[16] =
    {
        '0', '1', '2', '3',
        '4', '5', '6', '7',
        '8', '9', 'A', 'B',
        'C', 'D', 'E', 'F'
    };

    SvcLog_AppendChar(p_buffer, hex_table[(uint8_t)((value >> 4) & 0x0FU)]);
    SvcLog_AppendChar(p_buffer, hex_table[(uint8_t)(value & 0x0FU)]);
}

void SvcLog_AppendHex16(SvcLog_Buffer_t * p_buffer, uint16_t value)
{
    SvcLog_AppendHex8(p_buffer, (uint8_t)((value >> 8) & 0x00FFU));
    SvcLog_AppendHex8(p_buffer, (uint8_t)(value & 0x00FFU));
}

void SvcLog_AppendBytes(SvcLog_Buffer_t * p_buffer, const uint8_t * p_data, uint8_t length)
{
    uint8_t index;

    if ((p_buffer == NULL) || (p_data == NULL))
    {
        return;
    }

    for (index = 0U; index < length; index++)
    {
        SvcLog_AppendHex8(p_buffer, p_data[index]);

        if (index < (length - 1U))
        {
            SvcLog_AppendChar(p_buffer, ' ');
        }
    }
}

void SvcLog_AppendCrLf(SvcLog_Buffer_t * p_buffer)
{
    SvcLog_AppendChar(p_buffer, '\r');
    SvcLog_AppendChar(p_buffer, '\n');
}

void SvcLog_Transmit(const SvcLog_Buffer_t * p_buffer)
{
    if ((gp_uart != NULL) && (p_buffer != NULL))
    {
        (void)HAL_UART_Transmit(gp_uart,
                                (uint8_t *)(void *)p_buffer->data,
                                p_buffer->length,
                                100U);
    }
}

void SvcLog_WriteSeparator(void)
{
    SvcLog_Buffer_t buffer;

    SvcLog_BufferInit(&buffer);
    SvcLog_AppendCrLf(&buffer);
    SvcLog_AppendText(&buffer, "--------------------------------------------------");
    SvcLog_AppendCrLf(&buffer);
    SvcLog_Transmit(&buffer);
}

void SvcLog_WriteTrace(uint32_t timestamp_ms,
                       const char * p_channel,
                       const char * p_direction,
                       uint16_t standard_id,
                       uint8_t dlc,
                       uint8_t fd_enabled,
                       uint8_t brs_enabled,
                       const uint8_t * p_data,
                       uint8_t length)
{
    SvcLog_Buffer_t buffer;

    SvcLog_BufferInit(&buffer);
    SvcLog_AppendU32(&buffer, timestamp_ms);
    SvcLog_AppendText(&buffer, " ");
    SvcLog_AppendText(&buffer, p_channel);
    SvcLog_AppendText(&buffer, " ");
    SvcLog_AppendText(&buffer, p_direction);
    SvcLog_AppendText(&buffer, " ID=");
    SvcLog_AppendHex16(&buffer, standard_id);
    SvcLog_AppendText(&buffer, " DLC=");
    SvcLog_AppendU32(&buffer, dlc);
    SvcLog_AppendText(&buffer, " ");

    if (fd_enabled == 1U)
    {
        SvcLog_AppendText(&buffer, "FD");
    }
    else
    {
        SvcLog_AppendText(&buffer, "CAN");
    }

    SvcLog_AppendText(&buffer, " ");

    if (brs_enabled == 1U)
    {
        SvcLog_AppendText(&buffer, "BRS");
    }
    else
    {
        SvcLog_AppendText(&buffer, "NOBRS");
    }

    SvcLog_AppendText(&buffer, " DATA=");
    SvcLog_AppendBytes(&buffer, p_data, length);
    SvcLog_AppendCrLf(&buffer);
    SvcLog_Transmit(&buffer);
}
