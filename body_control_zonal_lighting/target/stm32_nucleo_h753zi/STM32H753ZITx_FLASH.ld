/* STM32H753ZIT6 — 2 MB flash, 1 MB DTCMRAM, 512 KB AXI SRAM (D1)
 * 288 KB SRAM D2 (SRAM1 + SRAM2), 64 KB SRAM D3, 64 KB ITCMRAM          */

ENTRY(Reset_Handler)

_estack         = ORIGIN(DTCMRAM) + LENGTH(DTCMRAM);  /* top of stack */
_Min_Heap_Size  = 0x2000;   /* 8 KB heap  */
_Min_Stack_Size = 0x1000;   /* 4 KB stack */

MEMORY
{
    ITCMRAM  (xrw) : ORIGIN = 0x00000000, LENGTH = 64K
    FLASH    (rx)  : ORIGIN = 0x08000000, LENGTH = 2048K
    DTCMRAM  (xrw) : ORIGIN = 0x20000000, LENGTH = 128K
    RAM_D1   (xrw) : ORIGIN = 0x24000000, LENGTH = 512K
    RAM_D2   (xrw) : ORIGIN = 0x30000000, LENGTH = 288K
    RAM_D3   (xrw) : ORIGIN = 0x38000000, LENGTH = 64K
    BKPRAM   (xrw) : ORIGIN = 0x38800000, LENGTH = 4K
}

SECTIONS
{
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } >FLASH

    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text*)
        *(.glue_7)
        *(.glue_7t)
        *(.eh_frame)
        KEEP(*(.init))
        KEEP(*(.fini))
        . = ALIGN(4);
        _etext = .;
    } >FLASH

    .rodata :
    {
        . = ALIGN(4);
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
    } >FLASH

    .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >FLASH
    .ARM        : { __exidx_start = .; *(.ARM.exidx*) __exidx_end = .; } >FLASH

    .preinit_array :
    {
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array*))
        PROVIDE_HIDDEN(__preinit_array_end = .);
    } >FLASH

    .init_array :
    {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array*))
        PROVIDE_HIDDEN(__init_array_end = .);
    } >FLASH

    .fini_array :
    {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array*))
        PROVIDE_HIDDEN(__fini_array_end = .);
    } >FLASH

    /* ETH DMA descriptors and RX buffers in RAM_D2 (accessible by ETH DMA).
     * 32-byte alignment matches the M7 D-cache line so that cache maintenance
     * (SCB_InvalidateDCache_by_Addr) operates on clean boundaries.           */
    .RxDecripSection (NOLOAD) :
    {
        . = ALIGN(32);
        *(.RxDecripSection)
        . = ALIGN(32);
    } >RAM_D2

    .TxDecripSection (NOLOAD) :
    {
        . = ALIGN(32);
        *(.TxDecripSection)
        . = ALIGN(32);
    } >RAM_D2

    .RxArraySection (NOLOAD) :
    {
        . = ALIGN(32);
        *(.RxArraySection)
        . = ALIGN(32);
    } >RAM_D2

    /* TX bounce buffer — single flat copy destination for low_level_output.
     * Placed in RAM_D2 (non-cacheable via MPU, DMA-accessible) because the
     * ETH DMA cannot reach DTCMRAM where LwIP pbuf payloads live.           */
    .TxScratchSection (NOLOAD) :
    {
        . = ALIGN(32);
        *(.TxScratchSection)
        . = ALIGN(32);
    } >RAM_D2

    _sidata = LOADADDR(.data);

    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } >DTCMRAM AT> FLASH

    .bss (NOLOAD) :
    {
        . = ALIGN(4);
        _sbss = .;
        __bss_start__ = _sbss;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = _ebss;
    } >DTCMRAM

    ._user_heap_stack :
    {
        . = ALIGN(8);
        PROVIDE(end = .);
        PROVIDE(_end = .);
        . = . + _Min_Heap_Size;
        . = . + _Min_Stack_Size;
        . = ALIGN(8);
    } >DTCMRAM

    /DISCARD/ : { libc.a(*) libm.a(*) libgcc.a(*) }

    .ARM.attributes 0 : { *(.ARM.attributes) }
}
