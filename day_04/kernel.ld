ENTRY(start)
__image_base = 0x100000;

SECTIONS
{
    . = 0;
    
    .text : { 
	*(.text.start)
	*(.text*)
    }

    .rdata : { *(.rdata*) }
    .data : { *(.data*) }
    .bss : { *(.bss*) }
    
    /DISCARD/ : {
        *(.eh_frame)
        *(.comment)
    }
}