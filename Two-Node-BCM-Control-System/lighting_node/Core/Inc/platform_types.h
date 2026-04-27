/**
 * @file platform_types.h
 * @brief MISRA-C:2012 Rule 6.1 / AUTOSAR Std_Types compliant boolean type.
 *
 * Defines bool_t, TRUE, and FALSE for use throughout the application layer.
 * All fields that carry only 0/1 semantics shall use bool_t; uint8_t is
 * reserved for values that are data (counters, enums, checksums, IDs).
 *
 * NOTE: g_bcm_ctx and g_lighting_ctx are static non-volatile.  In a
 * bare-metal superloop this is correct.  Should an interrupt handler or
 * FreeRTOS task ever read these structs, the shared boolean fields must be
 * wrapped in a critical section or declared volatile.
 */
#ifndef PLATFORM_TYPES_H
#define PLATFORM_TYPES_H

#include <stdint.h>

typedef uint8_t bool_t;

#define TRUE  ((bool_t)1U)
#define FALSE ((bool_t)0U)

#endif /* PLATFORM_TYPES_H */
