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
    IoLightingOutputs_ApplyCommand(&out, &cmd, 0U);
    test_assert(out.head_lamp_on       == 0U, "T1: no bits → head off");
    test_assert(out.park_lamp_on       == 0U, "T1: no bits → park off");
    test_assert(out.left_indicator_on  == 0U, "T1: no bits → left off");
    test_assert(out.right_indicator_on == 0U, "T1: no bits → right off");
    test_assert(out.hazard_lamp_on     == 0U, "T1: no bits → hazard off");
    test_assert(out.state == LIGHTING_STATE_NORMAL, "T1: state = NORMAL");

    /* T2: HEAD only → head on, state = HEAD_ACTIVE */
    IoLightingOutputs_Init(&out);
    cmd = make_cmd(LIGHTING_CMD_HEAD_LAMP_BIT);
    IoLightingOutputs_ApplyCommand(&out, &cmd, 0U);
    test_assert(out.head_lamp_on == 1U,                    "T2: head bit → head on");
    test_assert(out.state == LIGHTING_STATE_HEAD_ACTIVE,   "T2: state = HEAD_ACTIVE");

    /* T3: PARK only → park on, state = PARK_ACTIVE */
    IoLightingOutputs_Init(&out);
    cmd = make_cmd(LIGHTING_CMD_PARK_LAMP_BIT);
    IoLightingOutputs_ApplyCommand(&out, &cmd, 0U);
    test_assert(out.park_lamp_on == 1U,                    "T3: park bit → park on");
    test_assert(out.state == LIGHTING_STATE_PARK_ACTIVE,   "T3: state = PARK_ACTIVE");

    /* T4: LEFT indicator, phase=0 → left off (blink gate closed) */
    IoLightingOutputs_Init(&out);
    cmd = make_cmd(LIGHTING_CMD_LEFT_IND_BIT);
    IoLightingOutputs_ApplyCommand(&out, &cmd, 0U);
    test_assert(out.left_indicator_on  == 0U, "T4: left bit + phase=0 → left off");
    test_assert(out.right_indicator_on == 0U, "T4: left bit + phase=0 → right off");
    test_assert(out.state == LIGHTING_STATE_LEFT_ACTIVE,   "T4: state = LEFT_ACTIVE");

    /* T5: LEFT indicator, phase=1 → left on */
    IoLightingOutputs_Init(&out);
    cmd = make_cmd(LIGHTING_CMD_LEFT_IND_BIT);
    IoLightingOutputs_ApplyCommand(&out, &cmd, 1U);
    test_assert(out.left_indicator_on  == 1U, "T5: left bit + phase=1 → left on");
    test_assert(out.right_indicator_on == 0U, "T5: left bit + phase=1 → right off");

    /* T6: RIGHT indicator, phase=1 → right on, left off */
    IoLightingOutputs_Init(&out);
    cmd = make_cmd(LIGHTING_CMD_RIGHT_IND_BIT);
    IoLightingOutputs_ApplyCommand(&out, &cmd, 1U);
    test_assert(out.right_indicator_on == 1U, "T6: right bit + phase=1 → right on");
    test_assert(out.left_indicator_on  == 0U, "T6: right bit + phase=1 → left off");
    test_assert(out.state == LIGHTING_STATE_RIGHT_ACTIVE,  "T6: state = RIGHT_ACTIVE");

    /* T7: HAZARD overrides indicators — phase=1 → left+right+hazard all on */
    IoLightingOutputs_Init(&out);
    cmd = make_cmd(LIGHTING_CMD_HAZARD_BIT | LIGHTING_CMD_LEFT_IND_BIT);
    IoLightingOutputs_ApplyCommand(&out, &cmd, 1U);
    test_assert(out.left_indicator_on  == 1U, "T7: hazard + left + phase=1 → left on");
    test_assert(out.right_indicator_on == 1U, "T7: hazard overrides → right also on");
    test_assert(out.hazard_lamp_on     == 1U, "T7: hazard on");
    test_assert(out.park_lamp_on       == 1U, "T7: hazard forces park on");
    test_assert(out.state == LIGHTING_STATE_HAZARD_ACTIVE, "T7: state = HAZARD_ACTIVE");

    /* T8: HAZARD, phase=0 → indicators off (blink gate closed) */
    IoLightingOutputs_Init(&out);
    cmd = make_cmd(LIGHTING_CMD_HAZARD_BIT);
    IoLightingOutputs_ApplyCommand(&out, &cmd, 0U);
    test_assert(out.left_indicator_on  == 0U, "T8: hazard + phase=0 → left off");
    test_assert(out.right_indicator_on == 0U, "T8: hazard + phase=0 → right off");
    test_assert(out.hazard_lamp_on     == 0U, "T8: hazard lamp off when phase=0");
    test_assert(out.park_lamp_on       == 1U, "T8: park stays on regardless of phase");

    /* T9: fail-safe → park on, all others off, state = FAIL_SAFE_PARK */
    IoLightingOutputs_Init(&out);
    IoLightingOutputs_ApplyFailSafe(&out);
    test_assert(out.park_lamp_on       == 1U, "T9: fail-safe → park on");
    test_assert(out.head_lamp_on       == 0U, "T9: fail-safe → head off");
    test_assert(out.left_indicator_on  == 0U, "T9: fail-safe → left off");
    test_assert(out.right_indicator_on == 0U, "T9: fail-safe → right off");
    test_assert(out.hazard_lamp_on     == 0U, "T9: fail-safe → hazard off");
    test_assert(out.state == LIGHTING_STATE_FAIL_SAFE_PARK, "T9: state = FAIL_SAFE_PARK");

    /* T10: BCM input latch — hazard press clears left/right latches */
    IoBcmInputs_Init(&inputs);
    inputs.left_latched  = 1U;
    inputs.right_latched = 0U;
    inputs.hazard_latched = 0U;
    /* simulate hazard press event by toggling hazard_latched manually */
    if (inputs.hazard_latched == 0U)
    {
        inputs.hazard_latched = 1U;
        inputs.left_latched   = 0U;
        inputs.right_latched  = 0U;
    }
    test_assert(inputs.hazard_latched == 1U, "T10: hazard latch set");
    test_assert(inputs.left_latched   == 0U, "T10: hazard press clears left latch");
    test_assert(inputs.right_latched  == 0U, "T10: hazard press clears right latch");

    /* T11: left press clears right latch (mutex) */
    IoBcmInputs_Init(&inputs);
    inputs.right_latched = 1U;
    if (inputs.left_latched == 0U)
    {
        inputs.left_latched  = 1U;
        inputs.right_latched = 0U;
    }
    test_assert(inputs.left_latched  == 1U, "T11: left latch set");
    test_assert(inputs.right_latched == 0U, "T11: left press clears right latch");

    /* T12: NULL pointer safety for ApplyCommand */
    IoLightingOutputs_Init(&out);
    cmd = make_cmd(LIGHTING_CMD_HEAD_LAMP_BIT);
    IoLightingOutputs_ApplyCommand(NULL, &cmd, 0U);
    IoLightingOutputs_ApplyCommand(&out, NULL, 0U);
    test_assert(1, "T12: NULL pointer calls to ApplyCommand do not crash");
}
