# Phase 6 — STM32 NUCLEO-H753ZI hardware target

## Overview

Adds a cross-compilation path for the STM32H753ZI Cortex-M7 microcontroller.
The NUCLEO board acts as the **rear lighting node**: it receives SOME/IP lamp
commands from the Linux CentralZoneController over UDP/LwIP and drives five
GPIO outputs (PB0–PB4) wired to indicator and lamp circuits.

## New files

| File | Purpose |
|---|---|
| `cmake/arm-none-eabi-gcc.cmake` | CMake toolchain file for arm-none-eabi-gcc 13.2.1 (Cortex-M7, hard-FP) |
| `target/stm32_nucleo_h753zi/CMakeLists.txt` | HAL static library + startup INTERFACE target from STM32CubeH7 |
| `target/stm32_nucleo_h753zi/stm32h7xx_hal_conf.h` | Selects the HAL modules used (GPIO, ETH, UART, DMA, PWR, RCC) |
| `target/stm32_nucleo_h753zi/STM32H753ZITx_FLASH.ld` | Linker script: 2 MB flash, DTCMRAM heap/stack, RAM_D2 ETH DMA sections |
| `target/stm32_nucleo_h753zi/lwipopts.h` | LwIP 2.2 config: NO_SYS=1, UDP+ARP+ICMP, 8 KB heap, 8 Rx pbufs |
| `target/stm32_nucleo_h753zi/stm32h7xx_it.hpp/.cpp` | SysTick_Handler, ETH_IRQHandler, HardFault_Handler |
| `include/.../transport/lwip/lwip_udp_transport_adapter.hpp` | LwIP raw-UDP implementation of `TransportAdapterInterface` |
| `src/transport/lwip/lwip_udp_transport_adapter.cpp` | Encodes/decodes the project wire format using `lwip_htons/lwip_htonl` |
| `src/platform/stm32/ethernetif.cpp` | LwIP netif glue: `ethernetif_init`, `ethernetif_input`, TX callback |
| `app/stm32_nucleo_h753zi/main.cpp` | Bare-metal entry point: clock config, LwIP init, main polling loop |

## Modified files

| File | Change |
|---|---|
| `CMakeLists.txt` | Add `target/stm32_nucleo_h753zi` subdirectory for STM32; gate tests to Linux only |
| `src/CMakeLists.txt` | STM32 source list is now standalone (excludes threaded CZC/HMI); FetchContent for LwIP 2.2; link `lwip_stm32` + `stm32h7xx_hal` when STM32 |
| `app/CMakeLists.txt` | Gate Linux apps; add `body_control_stm32` ELF with post-build .bin/.hex |
| `src/platform/stm32/gpio_output_driver.cpp` | Add real `HAL_GPIO_WritePin` calls guarded by `#ifdef USE_HAL_DRIVER`; add `LampFunction→PinAssignment` lookup |

## GPIO pin map — NUCLEO-H753ZI

| `LampFunction` | Port/Pin | HAL constant | Board note |
|---|---|---|---|
| `kLeftIndicator` | PB0 | `GPIO_PIN_0` | LD1 green on-board LED |
| `kRightIndicator` | PB1 | `GPIO_PIN_1` | CN9 pin 25 |
| `kHazardLamp` | PB2 | `GPIO_PIN_2` | CN9 pin 22 (BOOT1 — safe as output during run) |
| `kParkLamp` | PB3 | `GPIO_PIN_3` | CN9 pin 21 |
| `kHeadLamp` | PB4 | `GPIO_PIN_4` | CN9 pin 19 |

No conflicts with Ethernet RMII pins (PA1/2/7, PC1/4/5, PB13, PG11/13).

## Network configuration (static)

| Parameter | Value |
|---|---|
| NUCLEO IP | 192.168.0.20 |
| CZC (Linux) IP | 192.168.0.10 |
| Subnet | /24 |
| Rear node UDP port | 41001 |
| CZC UDP port | 41000 |

## Build instructions

```bash
# Prerequisites: arm-none-eabi-gcc 13.2.1 on PATH,
#                STM32CubeH7 installed locally.

cmake -B build-stm32 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi-gcc.cmake \
  -DSTM32CUBEH7_DIR=/path/to/STM32Cube_FW_H7_Vx_y_z \
  -DBODY_CONTROL_LIGHTING_BUILD_TESTS=OFF \
  -DCMAKE_BUILD_TYPE=Debug

cmake --build build-stm32 -j

# Outputs: build-stm32/body_control_stm32.elf   (debug symbols)
#          build-stm32/body_control_stm32.bin    (for st-flash)
#          build-stm32/body_control_stm32.hex    (for STM32CubeProgrammer)
#          build-stm32/body_control_stm32.map    (linker map)
```

## Architecture notes

- LwIP runs in `NO_SYS=1` (bare-metal polling) mode. The main loop calls
  `ethernetif_input()` and `sys_check_timeouts()` every iteration.
- `LwipUdpTransportAdapter` is a drop-in replacement for the Linux
  `UdpTransportAdapter` — both implement `TransportAdapterInterface`. The
  service and application layers are untouched.
- The STM32 core source list deliberately excludes `CentralZoneController`
  (uses `std::thread`) and all HMI sources (POSIX/display). Only the domain,
  codec, service-provider, and platform drivers are compiled for bare-metal.
- ETH DMA descriptors and Rx buffers are placed in RAM_D2 (0x30000000)
  which the STM32H7 ETH DMA engine can access on the D2 bus.
