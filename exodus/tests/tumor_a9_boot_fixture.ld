ENTRY(_start)
PHDRS {
    code PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}
SECTIONS {
    . = 0xc0000000;
    .text : AT(0x10000) { *(.text*) } :code
    . = 0xc0001000;
    .data : AT(0x11000) { *(.data*) } :data
    .bss (NOLOAD) : AT(0x11004) { *(.bss*) } :data
    /DISCARD/ : { *(.ARM.attributes) *(.comment) *(.note*) }
}
