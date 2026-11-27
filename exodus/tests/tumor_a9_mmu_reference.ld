ENTRY(_start)
SECTIONS {
    . = 0x60010000;
    .text : { *(.text*) }
    /DISCARD/ : { *(.ARM.attributes) *(.comment) *(.note*) }
}
