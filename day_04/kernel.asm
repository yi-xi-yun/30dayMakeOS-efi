[BITS 64]
	EXTERN	kernel_main

[SECTION .text.start]
	; 该文件最终会与bootpack.c链接，生成kernel.exe，然后将二进制部分单独提取出作为kernel.bin
	; 使用这个特殊名称，是与kernel.ld配合，为了确保这个SECTION链接在最终二进制文件的开头
	GLOBAL	start
start:
	CLI
	MOV		RSP, 0x1ff000		; 内核用临时栈

	; 把Bootloader传入的BOOT_INFO(RDI)转交给WindowsABI的第一个参数(RCX)
	MOV		RCX, RDI
	SUB		RSP, 0x20
	CALL	kernel_main			; 64位下，C编译器生成的符号前不用加下划线
	ADD		RSP, 0x20
	
.fin:
	HLT
	JMP		.fin