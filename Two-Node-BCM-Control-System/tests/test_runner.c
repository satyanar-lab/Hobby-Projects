/*
 * test_runner.c — minimal host-side test harness
 *
 * Provides HAL stubs and a main() that runs all test suites.
 * Compile with:
 *   gcc -I../bcm_master/Core/Inc \
 *       test_runner.c test_heartbeat_timeout.c \
 *       test_checksum_validation.c test_lamp_arbitration.c \
 *       ../bcm_master/Core/Src/com_lighting_if.c \
 *       ../bcm_master/Core/Src/io_bcm_inputs.c \
 *       -o run_tests && ./run_tests
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

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

/* GPIO stub — all pins read as released (active-low → logical 0) */
typedef void GPIO_TypeDef;
typedef uint16_t uint16_t;

#define GPIO_PIN_RESET 0
#define GPIO_PIN_SET   1

int HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin)
{
    (void)port;
    (void)pin;
    return GPIO_PIN_SET;
}

void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, int state)
{
    (void)port;
    (void)pin;
    (void)state;
}

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
