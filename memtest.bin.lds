OUTPUT_FORMAT("binary")
OUTPUT_ARCH("i386")

ENTRY(_main);
SECTIONS {
	. = 0;
	.bootsect : { *(.bootsect) }
	.setup : { *(.setup) }
	.memtest : { 
		_start = . ;
		*(.data) 
		_end = . ;
	}
	_syssize = (_end - _start + 15) >> 4;
}
