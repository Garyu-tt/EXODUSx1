ENTRY(_start)
SECTIONS {
  . = 0x40080000;
  .text : { *(.text.boot) *(.text.guest) *(.text .text.*) }
  .rodata : { *(.rodata*) }
  .bss (NOLOAD) : { *(.bss*) }
  /DISCARD/ : { *(.comment) *(.note*) }
}
