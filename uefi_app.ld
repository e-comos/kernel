ENTRY(efi_main)

SECTIONS
{
  . = 0xffff800000000000 + 0x10000;

  .text ALIGN(0x1000) : {
	*(.text*)
  }

  .data ALIGN(0x1000) : {
	*(.data*)
	*(.sdata*)
  }

  .rodata ALIGN(0x1000) : {
	*(.rodata*)
	*(.srodata*)
  }

  .bss ALIGN(0x1000) (NOLOAD) : {
	*(.bss*)
	*(.sbss*)
	*(COMMON)
  }

  /DISCARD/ : { *(.note*) *(.comment*) }
}
