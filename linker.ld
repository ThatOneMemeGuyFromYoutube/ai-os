ENTRY(_start)
SECTIONS
{
  . = 1M;

  /* GRUB's Multiboot v1 header must be near the beginning of the image.
     Keep the complete header before the normal text section so GRUB can find it. */
  .multiboot ALIGN(4) :
  {
    KEEP(*(.multiboot))
  }

  .text : { *(.text*) }
  .rodata : { *(.rodata*) }
  .data : { *(.data*) }
  .bss : { *(COMMON) *(.bss*) }
}
