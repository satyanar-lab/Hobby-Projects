#include "app_bcm.h"
#include "io_bcm_inputs.h"
#include "lighting_messages.h"
#include "com_lighting_if.h"
#include "svc_log.h"
#include "canfd_spi.h"
#include "mcp2517fd_can.h"
#include "main.h"
#include "gpio.h"
#include "usart.h"
#include <stddef.h>

#ifndef LIGHTING_LINK_LED_GPIO_Port
#define LIGHTING_LINK_LED_GPIO_Port    REAR_LINK_LED_GPIO_Port
#define LIGHTING_LINK_LED_Pin          REAR_LINK_LED_Pin
#endif

#define APP_BCM_NODE_ID                    (1U)
#define APP_BCM_ALIVE_VALUE                (1U)
#define APP_BCM_TX_PERIOD_MS               (100U)
#define APP_BCM_STATUS_TIMEOUT_MS          (500U)
#define APP_BCM_LINK_LED_BLINK_MS          (120U)
#define APP_BCM_SUMMARY_PERIOD_MS          (500U)
#define APP_BCM_OSC_REGISTER_ADDRESS       (0x0E00U)
#define APP_BCM_OSC_READY_MASK             (0x00000400UL)

/*
 * NOTE — uint32_t tick arithmetic:
 *   All timeout checks use the form (now_ms - last_valid_tick) > THRESHOLD.
 *   uint32_t subtraction wraps correctly under unsigned arithmetic, so the
 *   comparison is safe across the ~49-day HAL_GetTick() rollover boundary.
 *   status_last_valid_tick is initialised to 0, meaning the very first
 *   cycle will satisfy the > 500 ms check; this is intentional — the node
 *   boots in status_timeout = TRUE until the first valid frame arrives.
 */
typedef struct
{
    IoBcmInputs_State_t          inputs;
    Lighting_BcmCommandFrame_t   command_frame;
    Lighting_StatusFrame_t       status_frame;
    bool_t   spi_ok;
    bool_t   node_ready;
    uint8_t  fd_mode;              /* MCP2517FD_MODE_* value — not bool */
    uint32_t tx_count;
    uint32_t last_tx_tick;
    uint32_t last_summary_tick;
    uint32_t status_last_valid_tick;
    bool_t   status_timeout;
    uint8_t  status_rx_checksum;   /* CRC-8 value — not bool */
    uint8_t  status_calc_checksum; /* CRC-8 value — not bool */
    bool_t   status_checksum_error;
    uint32_t status_checksum_ok_count;
    uint32_t status_checksum_fail_count;
    bool_t   link_led_state;
    uint32_t link_led_tick;
    uint32_t link_led_blink_count;
    uint8_t  last_logged_command_bits;        /* 0xFF sentinel — not bool */
    uint8_t  last_logged_status_timeout;      /* 0xFF sentinel — not bool */
    uint8_t  last_logged_status_checksum_error; /* 0xFF sentinel — not bool */
    uint8_t  last_logged_state;               /* 0xFF sentinel — not bool */
    uint8_t  last_logged_fail_safe;           /* 0xFF sentinel — not bool */
    uint8_t  last_logged_hb_timeout;          /* 0xFF sentinel — not bool */
} AppBcm_Context_t;

static AppBcm_Context_t g_bcm_ctx;
volatile AppBcm_Diag_t g_app_bcm_diag;

static void AppBcm_ClearDiag(void)
{
    g_app_bcm_diag.command_bits                = 0U;
    g_app_bcm_diag.heartbeat_counter           = 0U;
    g_app_bcm_diag.command_checksum            = 0U;
    g_app_bcm_diag.tx_count                    = 0UL;
    g_app_bcm_diag.lighting_status_timeout     = TRUE;
    g_app_bcm_diag.lighting_state             = LIGHTING_STATE_INIT;
    g_app_bcm_diag.lighting_fail_safe          = FALSE;
    g_app_bcm_diag.lighting_hb_timeout         = FALSE;
    g_app_bcm_diag.lighting_head_active        = FALSE;
    g_app_bcm_diag.lighting_park_active        = FALSE;
    g_app_bcm_diag.lighting_left_active        = FALSE;
    g_app_bcm_diag.lighting_right_active       = FALSE;
    g_app_bcm_diag.lighting_hazard_active      = FALSE;
    g_app_bcm_diag.status_checksum_error       = FALSE;
    g_app_bcm_diag.status_rx_checksum          = 0U;
    g_app_bcm_diag.status_calc_checksum        = 0U;
    g_app_bcm_diag.status_checksum_ok_count    = 0UL;
    g_app_bcm_diag.status_checksum_fail_count  = 0UL;
    g_app_bcm_diag.link_led_state              = FALSE;
    g_app_bcm_diag.link_led_blink_count        = 0UL;
}

static void AppBcm_UpdateDiag(void)
{
    g_app_bcm_diag.command_bits           = g_bcm_ctx.command_frame.command_bits;
    g_app_bcm_diag.heartbeat_counter      = g_bcm_ctx.command_frame.heartbeat_counter;
    g_app_bcm_diag.command_checksum       = g_bcm_ctx.command_frame.checksum;
    g_app_bcm_diag.tx_count               = g_bcm_ctx.tx_count;
    g_app_bcm_diag.lighting_status_timeout = g_bcm_ctx.status_timeout;
    g_app_bcm_diag.lighting_state         = g_bcm_ctx.status_frame.state;
    g_app_bcm_diag.lighting_fail_safe     =
        ComLighting_IsFlagSet(g_bcm_ctx.status_frame.flags, LIGHTING_STATUS_FAIL_SAFE_BIT);
    g_app_bcm_diag.lighting_hb_timeout    =
        ComLighting_IsFlagSet(g_bcm_ctx.status_frame.flags, LIGHTING_STATUS_HB_TIMEOUT_BIT);
    g_app_bcm_diag.lighting_head_active   =
        ComLighting_IsFlagSet(g_bcm_ctx.status_frame.flags, LIGHTING_STATUS_HEAD_ACTIVE_BIT);
    g_app_bcm_diag.lighting_park_active   =
        ComLighting_IsFlagSet(g_bcm_ctx.status_frame.flags, LIGHTING_STATUS_PARK_ACTIVE_BIT);
    g_app_bcm_diag.lighting_left_active   =
        ComLighting_IsFlagSet(g_bcm_ctx.status_frame.flags, LIGHTING_STATUS_LEFT_ACTIVE_BIT);
    g_app_bcm_diag.lighting_right_active  =
        ComLighting_IsFlagSet(g_bcm_ctx.status_frame.flags, LIGHTING_STATUS_RIGHT_ACTIVE_BIT);
    g_app_bcm_diag.lighting_hazard_active =
        ComLighting_IsFlagSet(g_bcm_ctx.status_frame.flags, LIGHTING_STATUS_HAZARD_ACTIVE_BIT);
    g_app_bcm_diag.status_checksum_error  = g_bcm_ctx.status_checksum_error;
    g_app_bcm_diag.status_rx_checksum     = g_bcm_ctx.status_rx_checksum;
    g_app_bcm_diag.status_calc_checksum   = g_bcm_ctx.status_calc_checksum;
    g_app_bcm_diag.status_checksum_ok_count   = g_bcm_ctx.status_checksum_ok_count;
    g_app_bcm_diag.status_checksum_fail_count = g_bcm_ctx.status_checksum_fail_count;
    g_app_bcm_diag.link_led_state         = g_bcm_ctx.link_led_state;
    g_app_bcm_diag.link_led_blink_count   = g_bcm_ctx.link_led_blink_count;
}

static void AppBcm_LogCommandEvent(void)
{
    SvcLog_Buffer_t log_buffer;
    SvcLog_WriteSeparator();
    SvcLog_BufferInit(&log_buffer);
    SvcLog_AppendU32(&log_buffer, HAL_GetTick());
    SvcLog_AppendText(&log_buffer, " APP BCM CMD H=");
    SvcLog_AppendU32(&log_buffer, g_bcm_ctx.inputs.head_latched);
    SvcLog_AppendText(&log_buffer, " P=");
    SvcLog_AppendU32(&log_buffer, g_bcm_ctx.inputs.park_latched);
    SvcLog_AppendText(&log_buffer, " L=");
    SvcLog_AppendU32(&log_buffer, g_bcm_ctx.inputs.left_latched);
    SvcLog_AppendText(&log_buffer, " R=");
    SvcLog_AppendU32(&log_buffer, g_bcm_ctx.inputs.right_latched);
    SvcLog_AppendText(&log_buffer, " Z=");
    SvcLog_AppendU32(&log_buffer, g_bcm_ctx.inputs.hazard_latched);
    SvcLog_AppendText(&log_buffer, " HB=");
    SvcLog_AppendU32(&log_buffer, g_bcm_ctx.command_frame.heartbeat_counter);
    SvcLog_AppendText(&log_buffer, " CS=");
    SvcLog_AppendHex8(&log_buffer, g_bcm_ctx.command_frame.checksum);
    SvcLog_AppendCrLf(&log_buffer);
    SvcLog_Transmit(&log_buffer);
}
static void AppBcm_LogStatusEvent(void)
{
    SvcLog_Buffer_t log_buffer;
    SvcLog_WriteSeparator();
    SvcLog_BufferInit(&log_buffer);
    SvcLog_AppendU32(&log_buffer, HAL_GetTick());
    SvcLog_AppendText(&log_buffer, " APP BCM LIGHT ST=");
    SvcLog_AppendU32(&log_buffer, g_bcm_ctx.status_frame.state);
    SvcLog_AppendText(&log_buffer, " FAIL=");
    SvcLog_AppendU32(&log_buffer, ComLighting_IsFlagSet(g_bcm_ctx.status_frame.flags, LIGHTING_STATUS_FAIL_SAFE_BIT));
    SvcLog_AppendText(&log_buffer, " HB_TO=");
    SvcLog_AppendU32(&log_buffer, ComLighting_IsFlagSet(g_bcm_ctx.status_frame.flags, LIGHTING_STATUS_HB_TIMEOUT_BIT));
    SvcLog_AppendText(&log_buffer, " H=");
    SvcLog_AppendU32(&log_buffer, ComLighting_IsFlagSet(g_bcm_ctx.status_frame.flags, LIGHTING_STATUS_HEAD_ACTIVE_BIT));
    SvcLog_AppendText(&log_buffer, " P=");
    SvcLog_AppendU32(&log_buffer, ComLighting_IsFlagSet(g_bcm_ctx.status_frame.flags, LIGHTING_STATUS_PARK_ACTIVE_BIT));
    SvcLog_AppendText(&log_buffer, " L=");
    SvcLog_AppendU32(&log_buffer, ComLighting_IsFlagSet(g_bcm_ctx.status_frame.flags, LIGHTING_STATUS_LEFT_ACTIVE_BIT));
    SvcLog_AppendText(&log_buffer, " R=");
    SvcLog_AppendU32(&log_buffer, ComLighting_IsFlagSet(g_bcm_ctx.status_frame.flags, LIGHTING_STATUS_RIGHT_ACTIVE_BIT));
    SvcLog_AppendText(&log_buffer, " Z=");
    SvcLog_AppendU32(&log_buffer, ComLighting_IsFlagSet(g_bcm_ctx.status_frame.flags, LIGHTING_STATUS_HAZARD_ACTIVE_BIT));
    SvcLog_AppendCrLf(&log_buffer);
    SvcLog_Transmit(&log_buffer);
}
static void AppBcm_LogStatusTimeout(void){SvcLog_Buffer_t b;SvcLog_WriteSeparator();SvcLog_BufferInit(&b);SvcLog_AppendU32(&b,HAL_GetTick());SvcLog_AppendText(&b," APP BCM LIGHT STATUS TIMEOUT");SvcLog_AppendCrLf(&b);SvcLog_Transmit(&b);}
static void AppBcm_LogStatusRestored(void){SvcLog_Buffer_t b;SvcLog_WriteSeparator();SvcLog_BufferInit(&b);SvcLog_AppendU32(&b,HAL_GetTick());SvcLog_AppendText(&b," APP BCM LIGHT STATUS RESTORED ST=");SvcLog_AppendU32(&b,g_bcm_ctx.status_frame.state);SvcLog_AppendText(&b," CNT=");SvcLog_AppendU32(&b,g_bcm_ctx.status_frame.status_counter);SvcLog_AppendCrLf(&b);SvcLog_Transmit(&b);}
static void AppBcm_LogChecksumFail(void){SvcLog_Buffer_t b;SvcLog_WriteSeparator();SvcLog_BufferInit(&b);SvcLog_AppendU32(&b,HAL_GetTick());SvcLog_AppendText(&b," APP BCM LIGHT STATUS CS FAIL RX=");SvcLog_AppendHex8(&b,g_bcm_ctx.status_rx_checksum);SvcLog_AppendText(&b," CALC=");SvcLog_AppendHex8(&b,g_bcm_ctx.status_calc_checksum);SvcLog_AppendText(&b," COUNT=");SvcLog_AppendU32(&b,g_bcm_ctx.status_checksum_fail_count);SvcLog_AppendCrLf(&b);SvcLog_Transmit(&b);}
static void AppBcm_LogChecksumOk(void){SvcLog_Buffer_t b;SvcLog_WriteSeparator();SvcLog_BufferInit(&b);SvcLog_AppendU32(&b,HAL_GetTick());SvcLog_AppendText(&b," APP BCM LIGHT STATUS CS OK COUNT=");SvcLog_AppendU32(&b,g_bcm_ctx.status_checksum_ok_count);SvcLog_AppendCrLf(&b);SvcLog_Transmit(&b);}
static void AppBcm_LogSummary(void){SvcLog_Buffer_t b;SvcLog_WriteSeparator();SvcLog_BufferInit(&b);SvcLog_AppendU32(&b,HAL_GetTick());SvcLog_AppendText(&b," APP BCM SUM CMD=");SvcLog_AppendHex8(&b,g_bcm_ctx.command_frame.command_bits);SvcLog_AppendText(&b," HB=");SvcLog_AppendU32(&b,g_bcm_ctx.command_frame.heartbeat_counter);SvcLog_AppendText(&b," TX=");SvcLog_AppendU32(&b,g_bcm_ctx.tx_count);SvcLog_AppendText(&b," TO=");SvcLog_AppendU32(&b,g_bcm_ctx.status_timeout);SvcLog_AppendText(&b," ST=");SvcLog_AppendU32(&b,g_bcm_ctx.status_frame.state);SvcLog_AppendText(&b," FAIL=");SvcLog_AppendU32(&b,ComLighting_IsFlagSet(g_bcm_ctx.status_frame.flags, LIGHTING_STATUS_FAIL_SAFE_BIT));SvcLog_AppendText(&b," CSERR=");SvcLog_AppendU32(&b,g_bcm_ctx.status_checksum_error);SvcLog_AppendText(&b," OK=");SvcLog_AppendU32(&b,g_bcm_ctx.status_checksum_ok_count);SvcLog_AppendText(&b," FAILCS=");SvcLog_AppendU32(&b,g_bcm_ctx.status_checksum_fail_count);SvcLog_AppendCrLf(&b);SvcLog_Transmit(&b);}

static void AppBcm_InitContext(void)
{
    IoBcmInputs_Init(&g_bcm_ctx.inputs);
    g_bcm_ctx.command_frame.command_bits      = 0U;
    g_bcm_ctx.command_frame.heartbeat_counter = 0U;
    g_bcm_ctx.command_frame.sender_node_id    = APP_BCM_NODE_ID;
    g_bcm_ctx.command_frame.bcm_alive         = APP_BCM_ALIVE_VALUE;
    g_bcm_ctx.command_frame.rolling_counter   = 0U;
    /* Fixed walking pattern in reserved bytes — aids oscilloscope verification
     * of payload byte alignment during bring-up and fills the 16-byte CAN FD
     * data phase.  Values are intentional, not accidental constants. */
    g_bcm_ctx.command_frame.reserved_5  = 0x11U;
    g_bcm_ctx.command_frame.reserved_6  = 0x22U;
    g_bcm_ctx.command_frame.reserved_7  = 0x33U;
    g_bcm_ctx.command_frame.reserved_8  = 0x44U;
    g_bcm_ctx.command_frame.reserved_9  = 0x55U;
    g_bcm_ctx.command_frame.reserved_10 = 0x66U;
    g_bcm_ctx.command_frame.reserved_11 = 0x77U;
    g_bcm_ctx.command_frame.reserved_12 = 0x88U;
    g_bcm_ctx.command_frame.reserved_13 = 0x99U;
    g_bcm_ctx.command_frame.reserved_14 = 0xAAU;
    g_bcm_ctx.status_frame.flags  = 0U;
    g_bcm_ctx.status_frame.state  = LIGHTING_STATE_INIT;
    g_bcm_ctx.spi_ok               = FALSE;
    g_bcm_ctx.node_ready           = FALSE;
    g_bcm_ctx.fd_mode              = 0U;
    g_bcm_ctx.tx_count             = 0UL;
    g_bcm_ctx.last_tx_tick         = 0UL;
    g_bcm_ctx.last_summary_tick    = 0UL;
    g_bcm_ctx.status_last_valid_tick     = 0UL;
    g_bcm_ctx.status_timeout             = TRUE;
    g_bcm_ctx.status_rx_checksum         = 0U;
    g_bcm_ctx.status_calc_checksum       = 0U;
    g_bcm_ctx.status_checksum_error      = FALSE;
    g_bcm_ctx.status_checksum_ok_count   = 0UL;
    g_bcm_ctx.status_checksum_fail_count = 0UL;
    g_bcm_ctx.link_led_state        = FALSE;
    g_bcm_ctx.link_led_tick         = 0UL;
    g_bcm_ctx.link_led_blink_count  = 0UL;
    /* 0xFF sentinel: ensures every real value triggers a log event on first cycle */
    g_bcm_ctx.last_logged_command_bits           = 0xFFU;
    g_bcm_ctx.last_logged_status_timeout         = 0xFFU;
    g_bcm_ctx.last_logged_status_checksum_error  = 0xFFU;
    g_bcm_ctx.last_logged_state                  = 0xFFU;
    g_bcm_ctx.last_logged_fail_safe              = 0xFFU;
    g_bcm_ctx.last_logged_hb_timeout             = 0xFFU;
    AppBcm_ClearDiag();
}

static void AppBcm_ConfigureController(void)
{
    HAL_StatusTypeDef local_status;
    uint32_t osc_value;
    uint8_t mode_value;
    osc_value  = 0UL;
    mode_value = 0U;

    HAL_GPIO_WritePin(LIGHTING_LINK_LED_GPIO_Port, LIGHTING_LINK_LED_Pin, GPIO_PIN_RESET);
    HAL_Delay(20U);
    CANFD_SPI_Deselect();
    HAL_Delay(5U);
    local_status = CANFD_SPI_Reset();
    HAL_Delay(10U);

    if (local_status == HAL_OK)
    {
        local_status = CANFD_SPI_ReadWord(APP_BCM_OSC_REGISTER_ADDRESS, &osc_value);
        if ((local_status == HAL_OK) && ((osc_value & APP_BCM_OSC_READY_MASK) != 0UL))
        {
            g_bcm_ctx.spi_ok     = TRUE;
            g_bcm_ctx.node_ready = TRUE;
        }
    }

    if (g_bcm_ctx.node_ready == TRUE)
    {
        local_status = MCP2517FD_CAN_RequestMode(MCP2517FD_MODE_CONFIG, &mode_value);
        if (local_status == HAL_OK){ local_status = MCP2517FD_CAN_SetNominalBitTiming_500k_20MHz(); }
        if (local_status == HAL_OK){ local_status = MCP2517FD_CAN_SetDataBitTiming_2M_20MHz(); }
        if (local_status == HAL_OK){ local_status = MCP2517FD_CAN_EnableTdcAuto(); }
        if (local_status == HAL_OK){ local_status = MCP2517FD_CAN_EnableBRS(); }
        if (local_status == HAL_OK){ local_status = MCP2517FD_CAN_ConfigureTxFifo1_16B(); }
        if (local_status == HAL_OK){ local_status = MCP2517FD_CAN_ConfigureRxFifo2_16B_TS(); }
        if (local_status == HAL_OK){ local_status = MCP2517FD_CAN_ConfigureFilter0_Std11_ToFifo2(LIGHTING_STATUS_ID); }
        if (local_status == HAL_OK){ local_status = MCP2517FD_CAN_RequestNormalFD(&g_bcm_ctx.fd_mode); }
    }
}

static void AppBcm_SendCommand(uint32_t now_ms)
{
    uint8_t payload[LIGHTING_FRAME_LENGTH];
    HAL_StatusTypeDef local_status;

    if ((now_ms - g_bcm_ctx.last_tx_tick) >= APP_BCM_TX_PERIOD_MS)
    {
        g_bcm_ctx.command_frame.command_bits     = IoBcmInputs_GetCommandBits(&g_bcm_ctx.inputs);
        g_bcm_ctx.command_frame.heartbeat_counter++;
        g_bcm_ctx.command_frame.rolling_counter  = (uint8_t)(g_bcm_ctx.tx_count & 0xFFU);

        ComLighting_EncodeBcmCommand(&g_bcm_ctx.command_frame, payload);
        g_bcm_ctx.command_frame.checksum = payload[15];

        local_status = MCP2517FD_CAN_TxFifo1SendFd16(LIGHTING_BCM_COMMAND_ID, payload, 1U);
        if (local_status == HAL_OK)
        {
            g_bcm_ctx.tx_count++;
            SvcLog_WriteTrace(now_ms, "CH1", "TX", LIGHTING_BCM_COMMAND_ID,
                              LIGHTING_FRAME_LENGTH, 1U, 1U, payload, LIGHTING_FRAME_LENGTH);
        }
        g_bcm_ctx.last_tx_tick = now_ms;
    }
}

static void AppBcm_PollStatus(uint32_t now_ms)
{
    uint8_t payload[LIGHTING_FRAME_LENGTH];
    uint16_t standard_id = 0U;
    uint8_t dlc_value    = 0U;
    uint8_t got_frame    = 0U;
    uint8_t fdf_value    = 0U;
    uint8_t brs_value    = 0U;
    HAL_StatusTypeDef local_status;
    bool_t valid_frame;

    local_status = MCP2517FD_CAN_RxFifo2PollFd16(&standard_id, payload,
                                                  &dlc_value, &fdf_value,
                                                  &brs_value, &got_frame);

    if ((local_status == HAL_OK) && (got_frame == 1U) &&
        (standard_id == LIGHTING_STATUS_ID) && (fdf_value == 1U))
    {
        SvcLog_WriteTrace(now_ms, "CH1", "RX", LIGHTING_STATUS_ID,
                          LIGHTING_FRAME_LENGTH, fdf_value, brs_value,
                          payload, LIGHTING_FRAME_LENGTH);
        valid_frame = ComLighting_DecodeLightingStatus(payload,
                                                       &g_bcm_ctx.status_frame,
                                                       &g_bcm_ctx.status_rx_checksum,
                                                       &g_bcm_ctx.status_calc_checksum);

        if (valid_frame == TRUE)
        {
            g_bcm_ctx.status_checksum_ok_count++;
            g_bcm_ctx.status_checksum_error  = FALSE;
            g_bcm_ctx.status_timeout         = FALSE;
            g_bcm_ctx.status_last_valid_tick  = now_ms;
        }
        else
        {
            g_bcm_ctx.status_checksum_fail_count++;
            g_bcm_ctx.status_checksum_error = TRUE;
        }
    }
}

static void AppBcm_UpdateStatusTimeout(uint32_t now_ms)
{
    if ((now_ms - g_bcm_ctx.status_last_valid_tick) > APP_BCM_STATUS_TIMEOUT_MS)
    {
        g_bcm_ctx.status_timeout = TRUE;
    }
}

static void AppBcm_UpdateLinkLed(uint32_t now_ms)
{
    bool_t fail_safe_active = ComLighting_IsFlagSet(g_bcm_ctx.status_frame.flags,
                                                    LIGHTING_STATUS_FAIL_SAFE_BIT);

    if (g_bcm_ctx.status_timeout == TRUE)
    {
        g_bcm_ctx.link_led_state = FALSE;
        HAL_GPIO_WritePin(LIGHTING_LINK_LED_GPIO_Port, LIGHTING_LINK_LED_Pin, GPIO_PIN_RESET);
    }
    else if (fail_safe_active == TRUE)
    {
        g_bcm_ctx.link_led_state = TRUE;
        HAL_GPIO_WritePin(LIGHTING_LINK_LED_GPIO_Port, LIGHTING_LINK_LED_Pin, GPIO_PIN_SET);
    }
    else if ((now_ms - g_bcm_ctx.link_led_tick) >= APP_BCM_LINK_LED_BLINK_MS)
    {
        g_bcm_ctx.link_led_tick  = now_ms;
        g_bcm_ctx.link_led_state = (g_bcm_ctx.link_led_state == FALSE) ? TRUE : FALSE;
        HAL_GPIO_WritePin(LIGHTING_LINK_LED_GPIO_Port,
                          LIGHTING_LINK_LED_Pin,
                          (g_bcm_ctx.link_led_state == TRUE) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        g_bcm_ctx.link_led_blink_count++;
    }
    else
    {
        /* no state change this cycle */
    }
}

static void AppBcm_ProcessLogs(uint32_t now_ms)
{
    bool_t fail_safe_active  = ComLighting_IsFlagSet(g_bcm_ctx.status_frame.flags,
                                                     LIGHTING_STATUS_FAIL_SAFE_BIT);
    bool_t hb_timeout_active = ComLighting_IsFlagSet(g_bcm_ctx.status_frame.flags,
                                                     LIGHTING_STATUS_HB_TIMEOUT_BIT);

    if (g_bcm_ctx.command_frame.command_bits != g_bcm_ctx.last_logged_command_bits)
    {
        AppBcm_LogCommandEvent();
        g_bcm_ctx.last_logged_command_bits = g_bcm_ctx.command_frame.command_bits;
    }

    if ((uint8_t)g_bcm_ctx.status_timeout != g_bcm_ctx.last_logged_status_timeout)
    {
        if (g_bcm_ctx.status_timeout == TRUE){ AppBcm_LogStatusTimeout(); }
        else { AppBcm_LogStatusRestored(); }
        g_bcm_ctx.last_logged_status_timeout = (uint8_t)g_bcm_ctx.status_timeout;
    }

    if ((uint8_t)g_bcm_ctx.status_checksum_error != g_bcm_ctx.last_logged_status_checksum_error)
    {
        if (g_bcm_ctx.status_checksum_error == TRUE){ AppBcm_LogChecksumFail(); }
        else { AppBcm_LogChecksumOk(); }
        g_bcm_ctx.last_logged_status_checksum_error = (uint8_t)g_bcm_ctx.status_checksum_error;
    }

    if (g_bcm_ctx.status_timeout == FALSE)
    {
        if ((g_bcm_ctx.status_frame.state    != g_bcm_ctx.last_logged_state)        ||
            ((uint8_t)fail_safe_active        != g_bcm_ctx.last_logged_fail_safe)   ||
            ((uint8_t)hb_timeout_active       != g_bcm_ctx.last_logged_hb_timeout))
        {
            AppBcm_LogStatusEvent();
            g_bcm_ctx.last_logged_state      = g_bcm_ctx.status_frame.state;
            g_bcm_ctx.last_logged_fail_safe  = (uint8_t)fail_safe_active;
            g_bcm_ctx.last_logged_hb_timeout = (uint8_t)hb_timeout_active;
        }
    }

    if ((now_ms - g_bcm_ctx.last_summary_tick) >= APP_BCM_SUMMARY_PERIOD_MS)
    {
        AppBcm_LogSummary();
        g_bcm_ctx.last_summary_tick = now_ms;
    }
}

void AppBcm_Init(void){ AppBcm_InitContext(); }
void AppBcm_Start(void)
{
    SvcLog_Init(&huart2);
    AppBcm_ConfigureController();
    SvcLog_WriteSeparator();
    { SvcLog_Buffer_t b; SvcLog_BufferInit(&b); SvcLog_AppendU32(&b, HAL_GetTick()); SvcLog_AppendText(&b, " APP BCM BOOT SPI="); SvcLog_AppendU32(&b, g_bcm_ctx.spi_ok); SvcLog_AppendText(&b, " FDMODE="); SvcLog_AppendU32(&b, g_bcm_ctx.fd_mode); SvcLog_AppendCrLf(&b); SvcLog_Transmit(&b); }
    AppBcm_UpdateDiag();
}
void AppBcm_RunCycle(void)
{
    uint32_t now_ms;
    if (g_bcm_ctx.fd_mode != MCP2517FD_MODE_NORMAL_FD)
    {
        AppBcm_UpdateDiag();
        return;
    }

    now_ms = HAL_GetTick();
    IoBcmInputs_Process(&g_bcm_ctx.inputs, now_ms);
    AppBcm_SendCommand(now_ms);
    AppBcm_PollStatus(now_ms);
    AppBcm_UpdateStatusTimeout(now_ms);
    AppBcm_UpdateLinkLed(now_ms);
    AppBcm_ProcessLogs(now_ms);
    AppBcm_UpdateDiag();
}
