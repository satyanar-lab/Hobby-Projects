#ifndef SVC_LOG_H
#define SVC_LOG_H

#include <stdint.h>
#include "usart.h"

#define SVC_LOG_BUFFER_SIZE    (256U)

typedef struct
{
    char data[SVC_LOG_BUFFER_SIZE];
    uint16_t length;
} SvcLog_Buffer_t;

void SvcLog_Init(UART_HandleTypeDef * p_uart);
void SvcLog_BufferInit(SvcLog_Buffer_t * p_buffer);
void SvcLog_AppendChar(SvcLog_Buffer_t * p_buffer, char value);
void SvcLog_AppendText(SvcLog_Buffer_t * p_buffer, const char * p_text);
void SvcLog_AppendU32(SvcLog_Buffer_t * p_buffer, uint32_t value);
void SvcLog_AppendHex8(SvcLog_Buffer_t * p_buffer, uint8_t value);
void SvcLog_AppendHex16(SvcLog_Buffer_t * p_buffer, uint16_t value);
void SvcLog_AppendBytes(SvcLog_Buffer_t * p_buffer, const uint8_t * p_data, uint8_t length);
void SvcLog_AppendCrLf(SvcLog_Buffer_t * p_buffer);
void SvcLog_Transmit(const SvcLog_Buffer_t * p_buffer);
void SvcLog_WriteSeparator(void);
void SvcLog_WriteTrace(uint32_t timestamp_ms,
                       const char * p_channel,
                       const char * p_direction,
                       uint16_t standard_id,
                       uint8_t dlc,
                       uint8_t fd_enabled,
                       uint8_t brs_enabled,
                       const uint8_t * p_data,
                       uint8_t length);

#endif /* SVC_LOG_H */
