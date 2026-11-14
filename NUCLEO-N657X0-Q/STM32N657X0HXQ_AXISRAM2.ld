/* Linker script for STM32N657X0 FSBL: no internal flash, so code and data both live in AXISRAM2.
   The boot ROM copies the image (after its 0x400 header) to 0x34180400; in dev boot the debugger loads it there. */

ENTRY(Reset_Handler)

MEMORY
{
  ROM    (xrw) : ORIGIN = 0x34180400, LENGTH = 255K  /* code + rodata + .data init values */
  RAM    (xrw) : ORIGIN = 0x341C0000, LENGTH = 256K  /* data/bss/stack */
}

_estack = ORIGIN(RAM) + LENGTH(RAM);    /* stack grows down from the top of RAM */
_Min_Stack_Size = 0x1000;               /* stack limit (MSPLIM) and overflow check below */
_sstack = _estack - _Min_Stack_Size;    /* loaded into MSPLIM by the startup code */

SECTIONS
{
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } >ROM

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
  } >ROM

  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)
    *(.rodata*)
    . = ALIGN(4);
  } >ROM

  .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >ROM
  .ARM.exidx :
  {
    __exidx_start = .;
    *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    __exidx_end = .;
  } >ROM

  /* Constructor/destructor tables walked by __libc_init_array in the startup code */
  .preinit_array :
  {
    PROVIDE_HIDDEN(__preinit_array_start = .);
    KEEP(*(.preinit_array*))
    PROVIDE_HIDDEN(__preinit_array_end = .);
  } >ROM
  .init_array :
  {
    PROVIDE_HIDDEN(__init_array_start = .);
    KEEP(*(SORT(.init_array.*)))
    KEEP(*(.init_array*))
    PROVIDE_HIDDEN(__init_array_end = .);
  } >ROM
  .fini_array :
  {
    PROVIDE_HIDDEN(__fini_array_start = .);
    KEEP(*(SORT(.fini_array.*)))
    KEEP(*(.fini_array*))
    PROVIDE_HIDDEN(__fini_array_end = .);
  } >ROM

  /* TrustZone secure-gateway veneers (SystemInit exports a non-secure-callable entry) */
  .gnu.sgstubs :
  {
    . = ALIGN(32);
    *(.gnu.sgstubs*)
    . = ALIGN(32);
  } >ROM

  /* Initialised data: values stored in ROM region and copied at reset */
  _sidata = LOADADDR(.data);
  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data)
    *(.data*)
    . = ALIGN(4);
    _edata = .;
  } >RAM AT> ROM

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
  } >RAM

  /* Heap starts here (newlib's _sbrk uses 'end'); fail the link if the stack would not fit */
  ._user_heap_stack (NOLOAD) :
  {
    . = ALIGN(8);
    PROVIDE(end = .);
    PROVIDE(_end = .);
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM

  .ARM.attributes 0 : { *(.ARM.attributes) }
}
