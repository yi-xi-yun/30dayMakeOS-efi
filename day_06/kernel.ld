ENTRY(kernel_main)

SECTIONS
{    
    . = 0x200000;
    .text : {
        *(.text.start)
        *(.text*)
    }
    
    .rdata : { *(.rdata*) }
    .data : { 
	*(.data*)
	*(.bss*)
	*(COMMON)
    }
    .bss (NOLOAD) : {
        _has_bss = ABSOLUTE(.);
    }
    
    /DISCARD/ : {
        *(.eh_frame)
        *(.comment)
	*(.xdata)
	*(.pdata)
    }
}