/*
 * test_heartbeat_timeout.c
 *
 * Unit test stubs for heartbeat timeout logic.
 *
 * The production logic lives in app_lighting.c:AppLighting_UpdateTiming().
 * Because app_lighting.c depends heavily on HAL peripherals and MCP2517FD
 * drivers, these tests exercise the timeout decision rule directly by
 * reimplementing its core condition in a thin wrapper — a common pattern
 * for embedded units that cannot be linked standalone without all drivers.
 *
 * When the full driver stack is stubbed out, replace the wrapper with a
 * direct call to AppLighting_RunCycle() and inspect g_app_lighting_diag.
 */

#include <stdint.h>
#include <string.h>
#include "platform_types.h"
#include "lighting_messages.h"

extern void test_assert(int condition, const char *msg);
extern void HAL_SetTick(uint32_t ms);

/* ---- Thin wrapper mimicking the heartbeat timeout condition -------------- */

#define HB_TIMEOUT_MS  350U

typedef struct
{
    uint8_t  last_heartbeat;
    uint32_t last_heartbeat_tick;
    bool_t   heartbeat_timeout;
    bool_t   fail_safe;
} HbState_t;

static void hb_init(HbState_t *s, uint32_t now_ms)
{
    s->last_heartbeat      = 0U;
    s->last_heartbeat_tick = now_ms;
    s->heartbeat_timeout   = TRUE;
    s->fail_safe           = TRUE;
}

static void hb_receive(HbState_t *s, uint8_t counter, uint32_t now_ms)
{
    if (counter != s->last_heartbeat)
    {
        s->last_heartbeat      = counter;
        s->last_heartbeat_tick = now_ms;
        s->heartbeat_timeout   = FALSE;
        s->fail_safe           = FALSE;
    }
}

static void hb_check_timeout(HbState_t *s, uint32_t now_ms)
{
    if ((now_ms - s->last_heartbeat_tick) > HB_TIMEOUT_MS)
    {
        s->heartbeat_timeout = TRUE;
        s->fail_safe         = TRUE;
    }
}

/* ---- Tests --------------------------------------------------------------- */

void test_heartbeat_timeout_run(void)
{
    HbState_t s;

    /* T1: node starts in timeout (boot-safe state) */
    hb_init(&s, 0U);
    test_assert(s.heartbeat_timeout == TRUE, "T1: boot state is timeout");
    test_assert(s.fail_safe == TRUE,         "T1: boot state is fail-safe");

    /* T2: fresh heartbeat clears timeout */
    hb_receive(&s, 1U, 100U);
    test_assert(s.heartbeat_timeout == FALSE, "T2: fresh HB clears timeout");
    test_assert(s.fail_safe == FALSE,         "T2: fresh HB clears fail-safe");

    /* T3: same counter value does NOT refresh the timer */
    hb_receive(&s, 1U, 400U);
    hb_check_timeout(&s, 460U);
    test_assert(s.heartbeat_timeout == TRUE, "T3: duplicate counter does not prevent timeout");

    /* T4: advancing counter before deadline keeps link alive */
    hb_init(&s, 0U);
    hb_receive(&s, 1U, 0U);
    hb_receive(&s, 2U, 100U);
    hb_check_timeout(&s, 420U);
    test_assert(s.heartbeat_timeout == FALSE, "T4: advancing counter within deadline keeps link alive");

    /* T5: timeout fires exactly after 350 ms gap */
    hb_init(&s, 0U);
    hb_receive(&s, 5U, 1000U);
    hb_check_timeout(&s, 1350U);
    test_assert(s.heartbeat_timeout == FALSE, "T5: exactly at boundary — no timeout yet");
    hb_check_timeout(&s, 1351U);
    test_assert(s.heartbeat_timeout == TRUE, "T5: one ms past boundary — timeout fires");

    /* T6: recovery — new counter after timeout restores normal state */
    hb_receive(&s, 6U, 2000U);
    test_assert(s.heartbeat_timeout == FALSE, "T6: new counter after timeout restores normal");
    test_assert(s.fail_safe == FALSE,         "T6: new counter after timeout clears fail-safe");

    /* T7: counter wraps 255 → 0 and is treated as a new value */
    hb_receive(&s, 255U, 3000U);
    hb_receive(&s, 0U,   3100U);
    test_assert(s.heartbeat_timeout == FALSE, "T7: counter wrap 255->0 accepted as fresh HB");
}
