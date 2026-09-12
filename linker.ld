ENTRY(_start)
SECTIONS
{
  . = 1M;

  /* The Multiboot header must be among the first 8192 bytes of the ELF
     file.  Put the boot text section first so the header is emitted before
     the rest of the kernel sections. */
  .text :
  {
    KEEP(*(.text.boot))
    *(.text*)
  }

  .rodata : { *(.rodata*) }
  .data : { *(.data*) }
  .bss : { *(COMMON) *(.bss*) }
}
