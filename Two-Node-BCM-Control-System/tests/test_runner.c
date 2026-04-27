/*
 * test_runner.c — minimal host-side test harness
 *
 * Provides HAL stubs and a main() that runs all test suites.
 * Compile with:
 *   gcc -I./stubs -I../bcm_master/Core/Inc -I../lighting_node/Core/Inc \
 *       test_runner.c test_heartbeat_timeout.c \
 *       test_checksum_validation.c test_lamp_arbitration.c \
 *       ../bcm_master/Core/Src/com_lighting_if.c \
 *       ../bcm_master/Core/Src/io_bcm_inputs.c \
 *       ../lighting_node/Core/Src/io_lighting_outputs.c \
 *       -o run_tests && ./run_tests
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* Pull in the HAL type definitions (GPIO_TypeDef, GPIO_PinState, etc.)
 * from the stub header so all stubs below match the declared signatures. */
#include "stm32l1xx_hal.h"

/* ---- HAL stubs ----------------------------------------------------------- */

static uint32_t g_tick_ms = 0;

uint32_t HAL_GetTick(void)
{
    return g_tick_ms;
}

void HAL_SetTick(uint32_t ms)
{
    g_tick_ms = ms;
}

void HAL_Delay(uint32_t ms)
{
    (void)ms;
}

/* All switch inputs read as released (active-low → GPIO_PIN_SET = not pressed) */
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin)
{
    (void)port;
    (void)pin;
    return GPIO_PIN_SET;
}

void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state)
{
    (void)port;
    (void)pin;
    (void)state;
}

void Error_Handler(void) {}
void MX_GPIO_Init(void) {}

/* ---- Assertion helper ---------------------------------------------------- */

static int g_pass = 0;
static int g_fail = 0;

void test_assert(int condition, const char *msg)
{
    if (condition)
    {
        printf("  PASS: %s\n", msg);
        g_pass++;
    }
    else
    {
        printf("  FAIL: %s\n", msg);
        g_fail++;
    }
}

/* ---- Forward declarations for test suites -------------------------------- */

void test_heartbeat_timeout_run(void);
void test_checksum_validation_run(void);
void test_lamp_arbitration_run(void);

/* ---- main ---------------------------------------------------------------- */

int main(void)
{
    printf("=== Two-Node BCM Unit Tests ===\n\n");

    printf("[heartbeat timeout]\n");
    test_heartbeat_timeout_run();

    printf("\n[checksum validation]\n");
    test_checksum_validation_run();

    printf("\n[lamp command arbitration]\n");
    test_lamp_arbitration_run();

    printf("\n==============================\n");
    printf("Results: %d passed, %d failed\n", g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
}
