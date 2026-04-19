set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER   arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)

set(CMAKE_OBJCOPY arm-none-eabi-objcopy CACHE FILEPATH "arm-none-eabi-objcopy")
set(CMAKE_SIZE    arm-none-eabi-size    CACHE FILEPATH "arm-none-eabi-size")

# STM32H753ZI — Cortex-M7 with double-precision hard-float FPU.
set(_BCL_MCU_FLAGS "-mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard")

set(CMAKE_C_FLAGS_INIT   "${_BCL_MCU_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${_BCL_MCU_FLAGS} -fno-exceptions -fno-rtti")
set(CMAKE_ASM_FLAGS_INIT "${_BCL_MCU_FLAGS} -x assembler-with-cpp")

# nosys.specs — stub syscalls; nano.specs — reduced newlib footprint.
set(CMAKE_EXE_LINKER_FLAGS_INIT
    "${_BCL_MCU_FLAGS} -specs=nosys.specs -specs=nano.specs -Wl,--gc-sections")

# Prevent CMake's linker sanity probe from failing before the vector table is in place.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
