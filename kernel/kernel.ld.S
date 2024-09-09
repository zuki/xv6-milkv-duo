#include "memlayout.h"

OUTPUT_ARCH( "riscv" )
ENTRY( _entry )

SECTIONS
{
  /*
   * ensure that entry.S / _entry is at KERNBASE,
   * where qemu's -kernel jumps.
   */
  . = KERNBASE;

  .text : {
    *(.text .text.*)
    . = ALIGN(0x1000);
    _trampoline = .;
    *(trampsec)
    . = ALIGN(0x1000);
    ASSERT(. - _trampoline == 0x1000, "error: trampoline larger than one page");
    PROVIDE(etext = .);
  }

  .rodata : {
    . = ALIGN(16);
    *(.srodata .srodata.*) /* do not need to distinguish this from .rodata */
    . = ALIGN(16);
    *(.rodata .rodata.*)
  }

  .data : {
    . = ALIGN(16);
    *(.sdata .sdata.*) /* do not need to distinguish this from .data */
    . = ALIGN(16);
    *(.data .data.*)
    . = ALIGN(0x1000);
    __ramdisk_start = .;
    *(.ramdisk .ramdisk.*)
    __ramdisk_end = .;
  }

  .bss : {
    . = ALIGN(16);
    _bss_start = .;
    *(.sbss .sbss.*) /* do not need to distinguish this from .bss */
    *(.bss .bss.*)
    . = ALIGN(16);
    _bss_end = .;
  }

  . = ALIGN(0x1000);
  PROVIDE(_end = .);
}
