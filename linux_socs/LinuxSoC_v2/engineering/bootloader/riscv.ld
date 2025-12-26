OUTPUT_ARCH("riscv")
ENTRY(_entry)

MEMORY
{
  flash (rx)  : ORIGIN = (0x20000000 + 1024*1024*6), LENGTH = 0x1000000
  ram   (rwx) : ORIGIN = 0x80000000 + (31*1024*1024), LENGTH = 1M

}

SECTIONS
{
  .init :
  {
    KEEP(*(.init))
  } > flash

  .text :
  {
    _text_start = .;
    KEEP(*(.vectors))
    *(.text*)
    *(.text.*)
    *libgcc.a:*(.text*)
    *libgcc.a:*(.text)
    *(.srodata*)
    *(.rodata*)
    *(.rodata.*)
    _text_end = .;
  } > ram AT > flash

  _text_load = LOADADDR(.text);

  .data :
  {
    _data_start = .;
    *(.data*)
    *(.sdata*)
    *libgcc.a:*(.data*)
    _edata = .;

    /* Define global pointer for RISC-V small-data ABI */
    PROVIDE(__global_pointer$ = _data_start + 0x800);
  } > ram AT > flash

  _data_load = LOADADDR(.data);

  .bss (NOLOAD) :
  {
    _bss_start = .;
    *(.bss*)
    *(.sbss*)
    *(COMMON)
    _ebss = .;
  } > ram

  /* Reserve heap space */
  . = ALIGN(16);
  _heap_start = .;
  . += 128K;
  _heap_end = .;

  /* ===== Stack configuration ===== */
  STACK_SIZE = 128K;                       /* <-- Stackgröße hier */
  _stack_top = ORIGIN(ram) + LENGTH(ram);
  _stack_bottom = _stack_top - STACK_SIZE;
  PROVIDE(_sp = _stack_top);

  /DISCARD/ :
  {
    *(.comment)
    *(.note*)
    *(.eh_frame*)
  }
}

