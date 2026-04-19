# ============================================================================
# NUCLEO-H753ZI board support library.
#
# Builds a minimal STM32H7xx HAL static library from an external CubeH7
# installation, plus startup/system files from the same CMSIS device package.
#
# Required cache variable:
#   STM32CUBEH7_DIR  — path to the STM32CubeH7 firmware package root
#                      (e.g. ~/STM32Cube/Repository/STM32Cube_FW_H7_V1.11.x)
# ============================================================================

if(NOT BODY_CONTROL_LIGHTING_TARGET_PLATFORM STREQUAL "stm32")
    return()
endif()

# --- Locate STM32CubeH7 -----------------------------------------------------

if(NOT DEFINED STM32CUBEH7_DIR OR STM32CUBEH7_DIR STREQUAL "")
    set(STM32CUBEH7_DIR "" CACHE PATH
        "Path to the STM32CubeH7 firmware package root directory")
endif()

if(NOT EXISTS
   "${STM32CUBEH7_DIR}/Drivers/STM32H7xx_HAL_Driver/Inc/stm32h7xx_hal.h")
    message(FATAL_ERROR
        "STM32CubeH7 not found at STM32CUBEH7_DIR='${STM32CUBEH7_DIR}'.\n"
        "Pass -DSTM32CUBEH7_DIR=/path/to/STM32Cube_FW_H7_Vx_y_z to cmake.")
endif()

set(_HAL_ROOT  "${STM32CUBEH7_DIR}/Drivers/STM32H7xx_HAL_Driver")
set(_CMSIS_DEV "${STM32CUBEH7_DIR}/Drivers/CMSIS/Device/ST/STM32H7xx")
set(_CMSIS_INC "${STM32CUBEH7_DIR}/Drivers/CMSIS/Include")

# --- Minimal HAL static library ---------------------------------------------

add_library(stm32h7xx_hal STATIC
    ${_HAL_ROOT}/Src/stm32h7xx_hal.c
    ${_HAL_ROOT}/Src/stm32h7xx_hal_cortex.c
    ${_HAL_ROOT}/Src/stm32h7xx_hal_dma.c
    ${_HAL_ROOT}/Src/stm32h7xx_hal_dma_ex.c
    ${_HAL_ROOT}/Src/stm32h7xx_hal_eth.c
    ${_HAL_ROOT}/Src/stm32h7xx_hal_eth_ex.c
    ${_HAL_ROOT}/Src/stm32h7xx_hal_gpio.c
    ${_HAL_ROOT}/Src/stm32h7xx_hal_pwr.c
    ${_HAL_ROOT}/Src/stm32h7xx_hal_pwr_ex.c
    ${_HAL_ROOT}/Src/stm32h7xx_hal_rcc.c
    ${_HAL_ROOT}/Src/stm32h7xx_hal_rcc_ex.c
    ${_HAL_ROOT}/Src/stm32h7xx_hal_uart.c
    ${_HAL_ROOT}/Src/stm32h7xx_hal_uart_ex.c
    ${_CMSIS_DEV}/Source/Templates/system_stm32h7xx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stm32h7xx_it.cpp
)

target_include_directories(stm32h7xx_hal
    PUBLIC
        ${_HAL_ROOT}/Inc
        ${_CMSIS_DEV}/Include
        ${_CMSIS_INC}
        ${CMAKE_CURRENT_SOURCE_DIR}   # stm32h7xx_hal_conf.h, stm32h7xx_it.hpp
)

target_compile_definitions(stm32h7xx_hal
    PUBLIC
        STM32H753xx
        USE_HAL_DRIVER
)

# Suppress warnings from the external HAL/CMSIS code.
target_compile_options(stm32h7xx_hal PRIVATE -w)

# --- Startup + linker-script interface target --------------------------------

add_library(stm32h753zi_startup INTERFACE)

target_sources(stm32h753zi_startup
    INTERFACE
        ${_CMSIS_DEV}/Source/Templates/gcc/startup_stm32h753xx.s
)

target_link_options(stm32h753zi_startup
    INTERFACE
        -T${CMAKE_CURRENT_SOURCE_DIR}/STM32H753ZITx_FLASH.ld
        -Wl,-Map=${CMAKE_BINARY_DIR}/body_control_stm32.map,--cref
)
