/*
 * stubs/gpio.h — stub for the CubeMX-generated gpio.h.
 * MX_GPIO_Init is not called in host-side tests; declaration provided to
 * satisfy the include chain: io_bcm_inputs.c → gpio.h → main.h.
 */
#ifndef __GPIO_H__
#define __GPIO_H__

#include "main.h"

void MX_GPIO_Init(void);

#endif /* __GPIO_H__ */
