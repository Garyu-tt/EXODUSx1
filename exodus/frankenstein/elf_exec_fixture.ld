ENTRY(_start)
PHDRS { text PT_LOAD FLAGS(5); data PT_LOAD FLAGS(6); }
SECTIONS {
  . = 0x3000;
  .text : { *(.text*) } :text
  . = 0x5000;
  .data : { *(.data*) } :data
  .bss (NOLOAD) : { *(.bss*) } :data
  /DISCARD/ : { *(.note*) *(.comment) }
}
