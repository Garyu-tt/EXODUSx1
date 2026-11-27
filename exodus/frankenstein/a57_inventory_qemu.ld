ENTRY(_start)
PHDRS {
  code PT_LOAD FLAGS(5);
  data PT_LOAD FLAGS(6);
}
SECTIONS {
  . = 0x40080000;
  .text : { *(.text*) *(.rodata*) } :code
  . = ALIGN(4096);
  .data : { *(.data*) *(.bss*) } :data
  /DISCARD/ : { *(.note*) *(.comment*) }
}
