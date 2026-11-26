ENTRY(_start)
SECTIONS {
  . = 0x60000000;
  .text : { *(.text.boot) *(.text*) }
  /DISCARD/ : { *(.comment) *(.note*) }
}
