ENTRY(_start)
PHDRS {
  text PT_LOAD FLAGS(5);
  ro PT_LOAD FLAGS(4);
  rw PT_LOAD FLAGS(6);
}
SECTIONS {
  . = 0x40080000;
  .text : { *(.text.boot) *(.text.guest) *(.text .text.*) } :text
  . = ALIGN(4096);
  .rodata : { *(.rodata*) *(.eh_frame*) } :ro
  . = ALIGN(4096);
  .data : { *(.data*) *(.got*) } :rw
  .bss (NOLOAD) : { *(.bss*) *(COMMON) } :rw
  /DISCARD/ : { *(.comment) *(.note*) }
}
