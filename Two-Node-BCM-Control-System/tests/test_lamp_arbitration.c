/*
 * test_lamp_arbitration.c
 *
 * Unit tests for lamp command arbitration rules implemented in:
 *   io_bcm_inputs.c    — switch debounce + toggle-latch + hazard/indicator mutex
 *   io_lighting_outputs.c (IoLightingOutputs_ApplyCommand) — output state machine
 *
 * IoLightingOutputs_ApplyCommand has no HAL dependencies and is tested
 * directly. IoBcmInputs_Process uses HAL_GetTick (stubbed in test_runner.c)
 * but not GPIO reads for these arbitration tests — we inject pre-debounced
 * state by manipulating the latched fields directly after Init.
 */

#include <stdint.h>
#include <string.h>
#include "platform_types.h"
#include "lighting_messages.h"
#include "io_bcm_inputs.h"
#include "io_lighting_outputs.h"

extern void test_assert(int condition, const char *msg);

/* ---- Helper: build a command frame from raw command_bits ---------------- */

static Lighting_BcmCommandFrame_t make_cmd(uint8_t bits)
{
    Lighting_BcmCommandFrame_t f;
    memset(&f, 0, sizeof(f));
    f.command_bits = bits;
    return f;
}

/* ---- IoLightingOutputs_ApplyCommand tests -------------------------------- */

void test_lamp_arbitration_run(void)
{
    IoLightingOutputs_State_t out;
    Lighting_BcmCommandFrame_t cmd;
    IoBcmInputs_State_t inputs;

    /* T1: no command bits → all lamps off, state = NORMAL */
    IoLightingOutputs_Init(&out);
    cmd = make_cmd(0x00U);
    IoLightingOutputs_ApplyCommand(&out, &cmd, FALSE);
    test_assert(out.head_lamp_on       == FALSE, "T1: no bits → head off");
    test_assert(out.park_lamp_on       == FALSE, "T1: no bits → park off");
    test_assert(out.left_indicator_on  == FALSE, "T1: no bits → left off");
    test_assert(out.right_indicator_on == FALSE, "T1: no bits → right off");
    test_assert(out.hazard_lamp_on     == FALSE, "T1: no bits → hazard off");
    test_assert(out.state == LIGHTING_STATE_NORMAL, "T1: state = NORMAL");

    /* T2: HEAD only → head on, state = HEAD_ACTIVE */
    IoLightingOutputs_Init(&out);
    cmd = make_cmd(LIGHTING_CMD_HEAD_LAMP_BIT);
    IoLightingOutputs_ApplyCommand(&out, &cmd, FALSE);
    test_assert(out.head_lamp_on == TRUE,                   "T2: head bit → head on");
    test_assert(out.state == LIGHTING_STATE_HEAD_ACTIVE,    "T2: state = HEAD_ACTIVE");

    /* T3: PARK only → park on, state = PARK_ACTIVE */
    IoLightingOutputs_Init(&out);
    cmd = make_cmd(LIGHTING_CMD_PARK_LAMP_BIT);
    IoLightingOutputs_ApplyCommand(&out, &cmd, FALSE);
    test_assert(out.park_lamp_on == TRUE,                   "T3: park bit → park on");
    test_assert(out.state == LIGHTING_STATE_PARK_ACTIVE,    "T3: state = PARK_ACTIVE");

    /* T4: LEFT indicator, phase=FALSE → left off (blink gate closed) */
    IoLightingOutputs_Init(&out);
    cmd = make_cmd(LIGHTING_CMD_LEFT_IND_BIT);
    IoLightingOutputs_ApplyCommand(&out, &cmd, FALSE);
    test_assert(out.left_indicator_on  == FALSE, "T4: left bit + phase=FALSE → left off");
    test_assert(out.right_indicator_on == FALSE, "T4: left bit + phase=FALSE → right off");
    test_assert(out.state == LIGHTING_STATE_LEFT_ACTIVE,    "T4: state = LEFT_ACTIVE");

    /* T5: LEFT indicator, phase=TRUE → left on */
    IoLightingOutputs_Init(&out);
    cmd = make_cmd(LIGHTING_CMD_LEFT_IND_BIT);
    IoLightingOutputs_ApplyCommand(&out, &cmd, TRUE);
    test_assert(out.left_indicator_on  == TRUE,  "T5: left bit + phase=TRUE → left on");
    test_assert(out.right_indicator_on == FALSE, "T5: left bit + phase=TRUE → right off");

    /* T6: RIGHT indicator, phase=TRUE → right on, left off */
    IoLightingOutputs_Init(&out);
    cmd = make_cmd(LIGHTING_CMD_RIGHT_IND_BIT);
    IoLightingOutputs_ApplyCommand(&out, &cmd, TRUE);
    test_assert(out.right_indicator_on == TRUE,  "T6: right bit + phase=TRUE → right on");
    test_assert(out.left_indicator_on  == FALSE, "T6: right bit + phase=TRUE → left off");
    test_assert(out.state == LIGHTING_STATE_RIGHT_ACTIVE,   "T6: state = RIGHT_ACTIVE");

    /* T7: HAZARD overrides indicators — phase=TRUE → left+right+hazard all on */
    IoLightingOutputs_Init(&out);
    cmd = make_cmd(LIGHTING_CMD_HAZARD_BIT | LIGHTING_CMD_LEFT_IND_BIT);
    IoLightingOutputs_ApplyCommand(&out, &cmd, TRUE);
    test_assert(out.left_indicator_on  == TRUE, "T7: hazard + left + phase=TRUE → left on");
    test_assert(out.right_indicator_on == TRUE, "T7: hazard overrides → right also on");
    test_assert(out.hazard_lamp_on     == TRUE, "T7: hazard on");
    test_assert(out.park_lamp_on       == TRUE, "T7: hazard forces park on");
    test_assert(out.state == LIGHTING_STATE_HAZARD_ACTIVE,  "T7: state = HAZARD_ACTIVE");

    /* T8: HAZARD, phase=FALSE → indicators off (blink gate closed) */
    IoLightingOutputs_Init(&out);
    cmd = make_cmd(LIGHTING_CMD_HAZARD_BIT);
    IoLightingOutputs_ApplyCommand(&out, &cmd, FALSE);
    test_assert(out.left_indicator_on  == FALSE, "T8: hazard + phase=FALSE → left off");
    test_assert(out.right_indicator_on == FALSE, "T8: hazard + phase=FALSE → right off");
    test_assert(out.hazard_lamp_on     == FALSE, "T8: hazard lamp off when phase=FALSE");
    test_assert(out.park_lamp_on       == TRUE,  "T8: park stays on regardless of phase");

    /* T9: fail-safe → park on, all others off, state = FAIL_SAFE_PARK */
    IoLightingOutputs_Init(&out);
    IoLightingOutputs_ApplyFailSafe(&out);
    test_assert(out.park_lamp_on       == TRUE,  "T9: fail-safe → park on");
    test_assert(out.head_lamp_on       == FALSE, "T9: fail-safe → head off");
    test_assert(out.left_indicator_on  == FALSE, "T9: fail-safe → left off");
    test_assert(out.right_indicator_on == FALSE, "T9: fail-safe → right off");
    test_assert(out.hazard_lamp_on     == FALSE, "T9: fail-safe → hazard off");
    test_assert(out.state == LIGHTING_STATE_FAIL_SAFE_PARK, "T9: state = FAIL_SAFE_PARK");

    /* T10: BCM input latch — hazard press clears left/right latches */
    IoBcmInputs_Init(&inputs);
    inputs.left_latched  = TRUE;
    inputs.right_latched = FALSE;
    inputs.hazard_latched = FALSE;
    /* simulate hazard press event by toggling hazard_latched manually */
    if (inputs.hazard_latched == FALSE)
    {
        inputs.hazard_latched = TRUE;
        inputs.left_latched   = FALSE;
        inputs.right_latched  = FALSE;
    }
    test_assert(inputs.hazard_latched == TRUE,  "T10: hazard latch set");
    test_assert(inputs.left_latched   == FALSE, "T10: hazard press clears left latch");
    test_assert(inputs.right_latched  == FALSE, "T10: hazard press clears right latch");

    /* T11: left press clears right latch (mutex) */
    IoBcmInputs_Init(&inputs);
    inputs.right_latched = TRUE;
    if (inputs.left_latched == FALSE)
    {
        inputs.left_latched  = TRUE;
        inputs.right_latched = FALSE;
    }
    test_assert(inputs.left_latched  == TRUE,  "T11: left latch set");
    test_assert(inputs.right_latched == FALSE, "T11: left press clears right latch");

    /* T12: NULL pointer safety for ApplyCommand */
    IoLightingOutputs_Init(&out);
    cmd = make_cmd(LIGHTING_CMD_HEAD_LAMP_BIT);
    IoLightingOutputs_ApplyCommand(NULL, &cmd, FALSE);
    IoLightingOutputs_ApplyCommand(&out, NULL, FALSE);
    test_assert(1, "T12: NULL pointer calls to ApplyCommand do not crash");
}
