#include "app_lighting.h"
#include "io_lighting_outputs.h"
#include "lighting_messages.h"
#include "com_lighting_if.h"
#include "svc_log.h"
#include "canfd_spi.h"
#include "mcp2517fd_can.h"
#include "main.h"
#include "gpio.h"
#include "usart.h"
#include <stddef.h>

#define APP_LIGHTING_NODE_ID               (2U)
#define APP_LIGHTING_HEARTBEAT_TIMEOUT_MS  (350U)
#define APP_LIGHTING_HEARTBEAT_LED_MS      (250U)
#define APP_LIGHTING_INDICATOR_MS          (330U)
#define APP_LIGHTING_STATUS_TX_MS          (100U)
#define APP_LIGHTING_SUMMARY_MS            (500U)
#define APP_LIGHTING_OSC_REGISTER_ADDRESS  (0x0E00U)
#define APP_LIGHTING_OSC_READY_MASK        (0x00000400UL)

typedef struct
{
    Lighting_BcmCommandFrame_t command_frame;
    Lighting_StatusFrame_t status_frame;
    IoLightingOutputs_State_t outputs;
    uint8_t spi_ok;
    uint8_t node_ready;
    uint8_t fd_mode;
    uint8_t last_heartbeat;
    uint32_t last_heartbeat_tick;
    uint8_t heartbeat_timeout;
    uint8_t fail_safe;
    uint8_t indicator_phase;
    uint32_t heartbeat_led_tick;
    uint32_t indicator_tick;
    uint32_t rx_count;
    uint32_t status_tx_count;
    uint32_t status_last_tx_tick;
    uint32_t last_summary_tick;
    uint8_t command_rx_checksum;
    uint8_t command_calc_checksum;
    uint8_t command_checksum_error;
    uint32_t command_checksum_ok_count;
    uint32_t command_checksum_fail_count;
    uint8_t status_checksum;
    uint8_t last_logged_command_bits;
    uint8_t last_logged_hb_timeout;
    uint8_t last_logged_fail_safe;
    uint8_t last_logged_state;
    uint8_t last_logged_checksum_error;
} AppLighting_Context_t;

static AppLighting_Context_t g_lighting_ctx;
volatile AppLighting_Diag_t g_app_lighting_diag;

static void AppLighting_UpdateDiag(void)
{
    g_app_lighting_diag.command_bits = g_lighting_ctx.command_frame.command_bits;
    g_app_lighting_diag.last_heartbeat = g_lighting_ctx.last_heartbeat;
    g_app_lighting_diag.command_checksum_error = g_lighting_ctx.command_checksum_error;
    g_app_lighting_diag.command_rx_checksum = g_lighting_ctx.command_rx_checksum;
    g_app_lighting_diag.command_calc_checksum = g_lighting_ctx.command_calc_checksum;
    g_app_lighting_diag.command_checksum_ok_count = g_lighting_ctx.command_checksum_ok_count;
    g_app_lighting_diag.command_checksum_fail_count = g_lighting_ctx.command_checksum_fail_count;
    g_app_lighting_diag.rx_count = g_lighting_ctx.rx_count;
    g_app_lighting_diag.lighting_state = g_lighting_ctx.outputs.state;
    g_app_lighting_diag.fail_safe = g_lighting_ctx.fail_safe;
    g_app_lighting_diag.heartbeat_timeout = g_lighting_ctx.heartbeat_timeout;
    g_app_lighting_diag.head_lamp_on = g_lighting_ctx.outputs.head_lamp_on;
    g_app_lighting_diag.park_lamp_on = g_lighting_ctx.outputs.park_lamp_on;
    g_app_lighting_diag.left_indicator_on = g_lighting_ctx.outputs.left_indicator_on;
    g_app_lighting_diag.right_indicator_on = g_lighting_ctx.outputs.right_indicator_on;
    g_app_lighting_diag.hazard_lamp_on = g_lighting_ctx.outputs.hazard_lamp_on;
    g_app_lighting_diag.heartbeat_led_on = g_lighting_ctx.outputs.heartbeat_led_on;
    g_app_lighting_diag.status_checksum = g_lighting_ctx.status_checksum;
    g_app_lighting_diag.status_tx_count = g_lighting_ctx.status_tx_count;
}

static void AppLighting_ClearDiag(void)
{
    g_app_lighting_diag.command_bits = 0U;
    g_app_lighting_diag.last_heartbeat = 0U;
    g_app_lighting_diag.command_checksum_error = 0U;
    g_app_lighting_diag.command_rx_checksum = 0U;
    g_app_lighting_diag.command_calc_checksum = 0U;
    g_app_lighting_diag.command_checksum_ok_count = 0UL;
    g_app_lighting_diag.command_checksum_fail_count = 0UL;
    g_app_lighting_diag.rx_count = 0UL;
    g_app_lighting_diag.lighting_state = LIGHTING_STATE_INIT;
    g_app_lighting_diag.fail_safe = 1U;
    g_app_lighting_diag.heartbeat_timeout = 1U;
    g_app_lighting_diag.head_lamp_on = 0U;
    g_app_lighting_diag.park_lamp_on = 0U;
    g_app_lighting_diag.left_indicator_on = 0U;
    g_app_lighting_diag.right_indicator_on = 0U;
    g_app_lighting_diag.hazard_lamp_on = 0U;
    g_app_lighting_diag.heartbeat_led_on = 0U;
    g_app_lighting_diag.status_checksum = 0U;
    g_app_lighting_diag.status_tx_count = 0UL;
}

/* log helpers simplified */
static void AppLighting_LogSimple(const char *prefix)
{
    SvcLog_Buffer_t b; SvcLog_WriteSeparator(); SvcLog_BufferInit(&b); SvcLog_AppendU32(&b, HAL_GetTick()); SvcLog_AppendText(&b, " "); SvcLog_AppendText(&b, prefix); SvcLog_AppendCrLf(&b); SvcLog_Transmit(&b);
}
static void AppLighting_LogCommandEvent(void){SvcLog_Buffer_t b; uint8_t h=((g_lighting_ctx.command_frame.command_bits & LIGHTING_CMD_HEAD_LAMP_BIT)!=0U)?1U:0U; uint8_t p=((g_lighting_ctx.command_frame.command_bits & LIGHTING_CMD_PARK_LAMP_BIT)!=0U)?1U:0U; uint8_t l=((g_lighting_ctx.command_frame.command_bits & LIGHTING_CMD_LEFT_IND_BIT)!=0U)?1U:0U; uint8_t r=((g_lighting_ctx.command_frame.command_bits & LIGHTING_CMD_RIGHT_IND_BIT)!=0U)?1U:0U; uint8_t z=((g_lighting_ctx.command_frame.command_bits & LIGHTING_CMD_HAZARD_BIT)!=0U)?1U:0U; SvcLog_WriteSeparator(); SvcLog_BufferInit(&b); SvcLog_AppendU32(&b,HAL_GetTick()); SvcLog_AppendText(&b," APP LIGHT CMD H="); SvcLog_AppendU32(&b,h); SvcLog_AppendText(&b," P="); SvcLog_AppendU32(&b,p); SvcLog_AppendText(&b," L="); SvcLog_AppendU32(&b,l); SvcLog_AppendText(&b," R="); SvcLog_AppendU32(&b,r); SvcLog_AppendText(&b," Z="); SvcLog_AppendU32(&b,z); SvcLog_AppendCrLf(&b); SvcLog_Transmit(&b);}
static void AppLighting_LogChecksumFail(void){SvcLog_Buffer_t b; SvcLog_WriteSeparator(); SvcLog_BufferInit(&b); SvcLog_AppendU32(&b,HAL_GetTick()); SvcLog_AppendText(&b," APP LIGHT BCM CMD CS FAIL RX="); SvcLog_AppendHex8(&b,g_lighting_ctx.command_rx_checksum); SvcLog_AppendText(&b," CALC="); SvcLog_AppendHex8(&b,g_lighting_ctx.command_calc_checksum); SvcLog_AppendText(&b," COUNT="); SvcLog_AppendU32(&b,g_lighting_ctx.command_checksum_fail_count); SvcLog_AppendCrLf(&b); SvcLog_Transmit(&b);}
static void AppLighting_LogChecksumOk(void){SvcLog_Buffer_t b; SvcLog_WriteSeparator(); SvcLog_BufferInit(&b); SvcLog_AppendU32(&b,HAL_GetTick()); SvcLog_AppendText(&b," APP LIGHT BCM CMD CS OK COUNT="); SvcLog_AppendU32(&b,g_lighting_ctx.command_checksum_ok_count); SvcLog_AppendCrLf(&b); SvcLog_Transmit(&b);}
static void AppLighting_LogHeartbeatTimeout(void){AppLighting_LogSimple("APP LIGHT HEARTBEAT TIMEOUT");}
static void AppLighting_LogHeartbeatRestored(void){SvcLog_Buffer_t b; SvcLog_WriteSeparator(); SvcLog_BufferInit(&b); SvcLog_AppendU32(&b,HAL_GetTick()); SvcLog_AppendText(&b," APP LIGHT HEARTBEAT RESTORED HB="); SvcLog_AppendU32(&b,g_lighting_ctx.last_heartbeat); SvcLog_AppendCrLf(&b); SvcLog_Transmit(&b);}
static void AppLighting_LogStateEvent(void){SvcLog_Buffer_t b; SvcLog_WriteSeparator(); SvcLog_BufferInit(&b); SvcLog_AppendU32(&b,HAL_GetTick()); SvcLog_AppendText(&b," APP LIGHT STATE="); SvcLog_AppendU32(&b,g_lighting_ctx.outputs.state); SvcLog_AppendText(&b," FAIL="); SvcLog_AppendU32(&b,g_lighting_ctx.fail_safe); SvcLog_AppendText(&b," H="); SvcLog_AppendU32(&b,g_lighting_ctx.outputs.head_lamp_on); SvcLog_AppendText(&b," P="); SvcLog_AppendU32(&b,g_lighting_ctx.outputs.park_lamp_on); SvcLog_AppendText(&b," L="); SvcLog_AppendU32(&b,g_lighting_ctx.outputs.left_indicator_on); SvcLog_AppendText(&b," R="); SvcLog_AppendU32(&b,g_lighting_ctx.outputs.right_indicator_on); SvcLog_AppendText(&b," Z="); SvcLog_AppendU32(&b,g_lighting_ctx.outputs.hazard_lamp_on); SvcLog_AppendCrLf(&b); SvcLog_Transmit(&b);}
static void AppLighting_LogSummary(void){SvcLog_Buffer_t b; SvcLog_WriteSeparator(); SvcLog_BufferInit(&b); SvcLog_AppendU32(&b,HAL_GetTick()); SvcLog_AppendText(&b," APP LIGHT SUM CMD="); SvcLog_AppendHex8(&b,g_lighting_ctx.command_frame.command_bits); SvcLog_AppendText(&b," HB="); SvcLog_AppendU32(&b,g_lighting_ctx.last_heartbeat); SvcLog_AppendText(&b," RX="); SvcLog_AppendU32(&b,g_lighting_ctx.rx_count); SvcLog_AppendText(&b," TX="); SvcLog_AppendU32(&b,g_lighting_ctx.status_tx_count); SvcLog_AppendText(&b," CSERR="); SvcLog_AppendU32(&b,g_lighting_ctx.command_checksum_error); SvcLog_AppendText(&b," ST="); SvcLog_AppendU32(&b,g_lighting_ctx.outputs.state); SvcLog_AppendText(&b," FAIL="); SvcLog_AppendU32(&b,g_lighting_ctx.fail_safe); SvcLog_AppendText(&b," HBTO="); SvcLog_AppendU32(&b,g_lighting_ctx.heartbeat_timeout); SvcLog_AppendText(&b," TXCS="); SvcLog_AppendHex8(&b,g_lighting_ctx.status_checksum); SvcLog_AppendCrLf(&b); SvcLog_Transmit(&b);}

static void AppLighting_InitContext(void)
{
    IoLightingOutputs_Init(&g_lighting_ctx.outputs);
    g_lighting_ctx.command_frame.command_bits = 0U;
    g_lighting_ctx.status_frame.diagnostic_pattern_a5 = 0xA5U;
    g_lighting_ctx.status_frame.node_id = APP_LIGHTING_NODE_ID;
    g_lighting_ctx.spi_ok = 0U;
    g_lighting_ctx.node_ready = 0U;
    g_lighting_ctx.fd_mode = 0U;
    g_lighting_ctx.last_heartbeat = 0U;
    g_lighting_ctx.last_heartbeat_tick = 0UL;
    g_lighting_ctx.heartbeat_timeout = 1U;
    g_lighting_ctx.fail_safe = 1U;
    g_lighting_ctx.indicator_phase = 0U;
    g_lighting_ctx.heartbeat_led_tick = 0UL;
    g_lighting_ctx.indicator_tick = 0UL;
    g_lighting_ctx.rx_count = 0UL;
    g_lighting_ctx.status_tx_count = 0UL;
    g_lighting_ctx.status_last_tx_tick = 0UL;
    g_lighting_ctx.last_summary_tick = 0UL;
    g_lighting_ctx.command_rx_checksum = 0U;
    g_lighting_ctx.command_calc_checksum = 0U;
    g_lighting_ctx.command_checksum_error = 0U;
    g_lighting_ctx.command_checksum_ok_count = 0UL;
    g_lighting_ctx.command_checksum_fail_count = 0UL;
    g_lighting_ctx.status_checksum = 0U;
    g_lighting_ctx.last_logged_command_bits = 0xFFU;
    g_lighting_ctx.last_logged_hb_timeout = 0xFFU;
    g_lighting_ctx.last_logged_fail_safe = 0xFFU;
    g_lighting_ctx.last_logged_state = 0xFFU;
    g_lighting_ctx.last_logged_checksum_error = 0xFFU;
    IoLightingOutputs_ApplyFailSafe(&g_lighting_ctx.outputs);
    g_lighting_ctx.outputs.heartbeat_led_on = 0U;
    AppLighting_ClearDiag();
}

static void AppLighting_ConfigureController(void)
{
    HAL_StatusTypeDef local_status;
    uint32_t osc_value = 0UL;
    uint8_t mode_value = 0U;

    HAL_Delay(20U);
    CANFD_SPI_Deselect();
    HAL_Delay(5U);
    local_status = CANFD_SPI_Reset();
    HAL_Delay(10U);

    if (local_status == HAL_OK)
    {
        local_status = CANFD_SPI_ReadWord(APP_LIGHTING_OSC_REGISTER_ADDRESS, &osc_value);
        if ((local_status == HAL_OK) && ((osc_value & APP_LIGHTING_OSC_READY_MASK) != 0UL))
        {
            g_lighting_ctx.spi_ok = 1U;
            g_lighting_ctx.node_ready = 1U;
        }
    }

    if (g_lighting_ctx.node_ready == 1U)
    {
        local_status = MCP2517FD_CAN_RequestMode(MCP2517FD_MODE_CONFIG, &mode_value);
        if (local_status == HAL_OK){ local_status = MCP2517FD_CAN_SetNominalBitTiming_500k_20MHz(); }
        if (local_status == HAL_OK){ local_status = MCP2517FD_CAN_SetDataBitTiming_2M_20MHz(); }
        if (local_status == HAL_OK){ local_status = MCP2517FD_CAN_EnableTdcAuto(); }
        if (local_status == HAL_OK){ local_status = MCP2517FD_CAN_EnableBRS(); }
        if (local_status == HAL_OK){ local_status = MCP2517FD_CAN_ConfigureTxFifo1_16B(); }
        if (local_status == HAL_OK){ local_status = MCP2517FD_CAN_ConfigureRxFifo2_16B_TS(); }
        if (local_status == HAL_OK){ local_status = MCP2517FD_CAN_ConfigureFilter0_Std11_ToFifo2(LIGHTING_BCM_COMMAND_ID); }
        if (local_status == HAL_OK){ local_status = MCP2517FD_CAN_RequestNormalFD(&g_lighting_ctx.fd_mode); }
    }
}

static void AppLighting_PollCommand(uint32_t now_ms)
{
    uint8_t payload[LIGHTING_FRAME_LENGTH];
    uint16_t standard_id = 0U;
    uint8_t dlc_value = 0U;
    uint8_t got_frame = 0U;
    uint8_t fdf_value = 0U;
    uint8_t brs_value = 0U;
    HAL_StatusTypeDef local_status;
    uint8_t valid_frame;

    local_status = MCP2517FD_CAN_RxFifo2PollFd16(&standard_id, payload, &dlc_value, &fdf_value, &brs_value, &got_frame);

    if ((local_status == HAL_OK) && (got_frame == 1U) && (standard_id == LIGHTING_BCM_COMMAND_ID) && (fdf_value == 1U))
    {
        SvcLog_WriteTrace(now_ms, "CH1", "RX", LIGHTING_BCM_COMMAND_ID, LIGHTING_FRAME_LENGTH, fdf_value, brs_value, payload, LIGHTING_FRAME_LENGTH);
        valid_frame = ComLighting_DecodeBcmCommand(payload, &g_lighting_ctx.command_frame, &g_lighting_ctx.command_rx_checksum, &g_lighting_ctx.command_calc_checksum);

        if (valid_frame == 1U)
        {
            g_lighting_ctx.command_checksum_ok_count++;
            g_lighting_ctx.command_checksum_error = 0U;
            g_lighting_ctx.rx_count++;

            if (g_lighting_ctx.command_frame.heartbeat_counter != g_lighting_ctx.last_heartbeat)
            {
                g_lighting_ctx.last_heartbeat = g_lighting_ctx.command_frame.heartbeat_counter;
                g_lighting_ctx.last_heartbeat_tick = now_ms;
                g_lighting_ctx.heartbeat_timeout = 0U;
                g_lighting_ctx.fail_safe = 0U;
            }
        }
        else
        {
            g_lighting_ctx.command_checksum_fail_count++;
            g_lighting_ctx.command_checksum_error = 1U;
        }
    }
}

static void AppLighting_UpdateTiming(uint32_t now_ms)
{
    if ((now_ms - g_lighting_ctx.last_heartbeat_tick) > APP_LIGHTING_HEARTBEAT_TIMEOUT_MS)
    {
        g_lighting_ctx.heartbeat_timeout = 1U;
        g_lighting_ctx.fail_safe = 1U;
        g_lighting_ctx.indicator_phase = 0U;
        IoLightingOutputs_ApplyFailSafe(&g_lighting_ctx.outputs);
        g_lighting_ctx.outputs.heartbeat_led_on = 0U;
    }
    else
    {
        if ((now_ms - g_lighting_ctx.heartbeat_led_tick) >= APP_LIGHTING_HEARTBEAT_LED_MS)
        {
            g_lighting_ctx.heartbeat_led_tick = now_ms;
            g_lighting_ctx.outputs.heartbeat_led_on ^= 1U;
        }

        if ((now_ms - g_lighting_ctx.indicator_tick) >= APP_LIGHTING_INDICATOR_MS)
        {
            g_lighting_ctx.indicator_tick = now_ms;
            g_lighting_ctx.indicator_phase ^= 1U;
        }

        IoLightingOutputs_ApplyCommand(&g_lighting_ctx.outputs,
                                       &g_lighting_ctx.command_frame,
                                       g_lighting_ctx.indicator_phase);
    }

    IoLightingOutputs_Write(&g_lighting_ctx.outputs);
}

static void AppLighting_SendStatus(uint32_t now_ms)
{
    uint8_t payload[LIGHTING_FRAME_LENGTH];
    HAL_StatusTypeDef local_status;

    if ((now_ms - g_lighting_ctx.status_last_tx_tick) >= APP_LIGHTING_STATUS_TX_MS)
    {
        g_lighting_ctx.status_frame.flags = 0U;

        if (g_lighting_ctx.outputs.head_lamp_on == 1U){ g_lighting_ctx.status_frame.flags |= LIGHTING_STATUS_HEAD_ACTIVE_BIT; }
        if (g_lighting_ctx.outputs.park_lamp_on == 1U){ g_lighting_ctx.status_frame.flags |= LIGHTING_STATUS_PARK_ACTIVE_BIT; }
        if (g_lighting_ctx.outputs.left_indicator_on == 1U){ g_lighting_ctx.status_frame.flags |= LIGHTING_STATUS_LEFT_ACTIVE_BIT; }
        if (g_lighting_ctx.outputs.right_indicator_on == 1U){ g_lighting_ctx.status_frame.flags |= LIGHTING_STATUS_RIGHT_ACTIVE_BIT; }
        if (g_lighting_ctx.outputs.hazard_lamp_on == 1U){ g_lighting_ctx.status_frame.flags |= LIGHTING_STATUS_HAZARD_ACTIVE_BIT; }
        if (g_lighting_ctx.heartbeat_timeout == 1U){ g_lighting_ctx.status_frame.flags |= LIGHTING_STATUS_HB_TIMEOUT_BIT; }
        if (g_lighting_ctx.fail_safe == 1U){ g_lighting_ctx.status_frame.flags |= LIGHTING_STATUS_FAIL_SAFE_BIT; }

        g_lighting_ctx.status_frame.state = g_lighting_ctx.outputs.state;
        g_lighting_ctx.status_frame.last_heartbeat_rx = g_lighting_ctx.last_heartbeat;
        g_lighting_ctx.status_frame.status_counter = (uint8_t)(g_lighting_ctx.status_tx_count & 0xFFU);
        g_lighting_ctx.status_frame.command_bits_echo = g_lighting_ctx.command_frame.command_bits;
        g_lighting_ctx.status_frame.rx_count_lsb = (uint8_t)(g_lighting_ctx.rx_count & 0xFFU);
        g_lighting_ctx.status_frame.heartbeat_led_state = g_lighting_ctx.outputs.heartbeat_led_on;
        g_lighting_ctx.status_frame.indicator_blink_state = g_lighting_ctx.indicator_phase;
        g_lighting_ctx.status_frame.head_command_echo = ((g_lighting_ctx.command_frame.command_bits & LIGHTING_CMD_HEAD_LAMP_BIT) != 0U) ? 1U : 0U;
        g_lighting_ctx.status_frame.park_command_echo = ((g_lighting_ctx.command_frame.command_bits & LIGHTING_CMD_PARK_LAMP_BIT) != 0U) ? 1U : 0U;
        g_lighting_ctx.status_frame.left_command_echo = ((g_lighting_ctx.command_frame.command_bits & LIGHTING_CMD_LEFT_IND_BIT) != 0U) ? 1U : 0U;
        g_lighting_ctx.status_frame.right_command_echo = ((g_lighting_ctx.command_frame.command_bits & LIGHTING_CMD_RIGHT_IND_BIT) != 0U) ? 1U : 0U;
        g_lighting_ctx.status_frame.hazard_command_echo = ((g_lighting_ctx.command_frame.command_bits & LIGHTING_CMD_HAZARD_BIT) != 0U) ? 1U : 0U;

        ComLighting_EncodeLightingStatus(&g_lighting_ctx.status_frame, payload);
        g_lighting_ctx.status_checksum = payload[15];
        g_lighting_ctx.status_frame.checksum = payload[15];

        local_status = MCP2517FD_CAN_TxFifo1SendFd16(LIGHTING_STATUS_ID, payload, 1U);
        if (local_status == HAL_OK)
        {
            g_lighting_ctx.status_tx_count++;
            SvcLog_WriteTrace(now_ms, "CH1", "TX", LIGHTING_STATUS_ID, LIGHTING_FRAME_LENGTH, 1U, 1U, payload, LIGHTING_FRAME_LENGTH);
        }
        g_lighting_ctx.status_last_tx_tick = now_ms;
    }
}

static void AppLighting_ProcessLogs(uint32_t now_ms)
{
    if (g_lighting_ctx.command_frame.command_bits != g_lighting_ctx.last_logged_command_bits)
    {
        AppLighting_LogCommandEvent();
        g_lighting_ctx.last_logged_command_bits = g_lighting_ctx.command_frame.command_bits;
    }

    if (g_lighting_ctx.command_checksum_error != g_lighting_ctx.last_logged_checksum_error)
    {
        if (g_lighting_ctx.command_checksum_error == 1U){ AppLighting_LogChecksumFail(); }
        else { AppLighting_LogChecksumOk(); }
        g_lighting_ctx.last_logged_checksum_error = g_lighting_ctx.command_checksum_error;
    }

    if (g_lighting_ctx.heartbeat_timeout != g_lighting_ctx.last_logged_hb_timeout)
    {
        if (g_lighting_ctx.heartbeat_timeout == 1U){ AppLighting_LogHeartbeatTimeout(); }
        else { AppLighting_LogHeartbeatRestored(); }
        g_lighting_ctx.last_logged_hb_timeout = g_lighting_ctx.heartbeat_timeout;
    }

    if ((g_lighting_ctx.outputs.state != g_lighting_ctx.last_logged_state) ||
        (g_lighting_ctx.fail_safe != g_lighting_ctx.last_logged_fail_safe))
    {
        AppLighting_LogStateEvent();
        g_lighting_ctx.last_logged_state = g_lighting_ctx.outputs.state;
        g_lighting_ctx.last_logged_fail_safe = g_lighting_ctx.fail_safe;
    }

    if ((now_ms - g_lighting_ctx.last_summary_tick) >= APP_LIGHTING_SUMMARY_MS)
    {
        AppLighting_LogSummary();
        g_lighting_ctx.last_summary_tick = now_ms;
    }
}

void AppLighting_Init(void){ AppLighting_InitContext(); }
void AppLighting_Start(void)
{
    SvcLog_Init(&huart2);
    AppLighting_ConfigureController();
    SvcLog_WriteSeparator();
    { SvcLog_Buffer_t b; SvcLog_BufferInit(&b); SvcLog_AppendU32(&b, HAL_GetTick()); SvcLog_AppendText(&b, " APP LIGHT BOOT SPI="); SvcLog_AppendU32(&b, g_lighting_ctx.spi_ok); SvcLog_AppendText(&b, " FDMODE="); SvcLog_AppendU32(&b, g_lighting_ctx.fd_mode); SvcLog_AppendCrLf(&b); SvcLog_Transmit(&b); }
    AppLighting_UpdateDiag();
}
void AppLighting_RunCycle(void)
{
    uint32_t now_ms;
    if (g_lighting_ctx.fd_mode != MCP2517FD_MODE_NORMAL_FD)
    {
        AppLighting_UpdateDiag();
        return;
    }

    now_ms = HAL_GetTick();
    AppLighting_PollCommand(now_ms);
    AppLighting_UpdateTiming(now_ms);
    AppLighting_SendStatus(now_ms);
    AppLighting_ProcessLogs(now_ms);
    AppLighting_UpdateDiag();
}
