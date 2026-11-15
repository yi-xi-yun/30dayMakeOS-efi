[BITS 64]
[DEFAULT REL]
[SECTION .text]
	GLOBAL	_EfiMain
_EfiMain:
	SUB	RSP, 0x28
	MOV	RCX, [RDX+0x40]
	LEA	RDX, [msg]
	CALL	QWORD [RCX+8]
fin:
	JMP	fin
[SECTION .data]
	ALIGN	8
msg:
	DW	__utf16__("Hello, world!"), 0x00
