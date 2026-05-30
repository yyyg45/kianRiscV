/* SPDX-License-Identifier: Apache-2.0 */
/* Copyright (c) 2026 Hirosh Dabui <hirosh@dabui.de> */

ENTRY(_start)

MEMORY {
  FLASH (rx)  : ORIGIN = 0x20100000, LENGTH = 0x10000000
  PSRAM (rwx) : ORIGIN = 0x807f8000, LENGTH = 0x00002000
}

__stacktop = ORIGIN(PSRAM) + LENGTH(PSRAM);

SECTIONS {
    .text : {
        . = ALIGN(4);
        *(.text)
        *(.text*)
        *(.rodata)
        *(.rodata*)
        _etext = .;
        _sidata = _etext;
    } >FLASH

    .data : AT(_sidata) {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } >FLASH

    .bss : {
        . = ALIGN(4);
        _sbss = .;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } >PSRAM

    .heap : {
        . = ALIGN(4);
        _heap_start = .;
        _end = .;
    } >PSRAM

    .misc : {
        . = ALIGN(4);
        *(.eh_frame)
        *(.eh_frame_hdr)
        *(.init_array)
        *(.gcc_except_table*)
    } >FLASH
}
