#ifndef LWIP_ARCH_CC_H
#define LWIP_ARCH_CC_H

/* LwIP architecture config for arm-none-eabi bare-metal.
 * Provides the compiler/platform bridge that lwip/arch.h expects. */

#include <stdint.h>
#include <stddef.h>

/* Byte order — Cortex-M7 is little-endian. */
#define BYTE_ORDER  LITTLE_ENDIAN

/* LwIP integer typedefs. */
typedef uint8_t   u8_t;
typedef int8_t    s8_t;
typedef uint16_t  u16_t;
typedef int16_t   s16_t;
typedef uint32_t  u32_t;
typedef int32_t   s32_t;
typedef uintptr_t mem_ptr_t;

/* printf format specifiers. */
#define U16_F  "u"
#define S16_F  "d"
#define X16_F  "x"
#define U32_F  "u"
#define S32_F  "d"
#define X32_F  "x"
#define SZT_F  "zu"

/* Packed struct support (GCC). */
#define PACK_STRUCT_FIELD(x)  x
#define PACK_STRUCT_STRUCT    __attribute__((packed))
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_END

/* Diagnostics — no stdout in bare-metal; silence the macros. */
#define LWIP_PLATFORM_DIAG(x)    do { } while (0)
#define LWIP_PLATFORM_ASSERT(x)  do { } while (0)

/* Critical section — NO_SYS=1, single-threaded polling; no protection needed. */
#define SYS_ARCH_DECL_PROTECT(x)
#define SYS_ARCH_PROTECT(x)
#define SYS_ARCH_UNPROTECT(x)

#endif  /* LWIP_ARCH_CC_H */
