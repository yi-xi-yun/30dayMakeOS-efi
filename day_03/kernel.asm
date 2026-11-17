[BITS 64]
	ORG		0x100000

BI_FB_BASE	EQU		0x08

	CLI
	MOV		RSP, 0x1ff000		; 内核用临时栈

	MOV		RBX, RDI
	MOV		R8, [RBX+BI_FB_BASE]
	TEST	R8, R8
	JZ		.fin
	
	XOR		RCX, RCX
	MOV		RDX, 0x100000
.loop:
	MOV		EAX, ECX
	AND		EAX, 0x0f
	SHL		EAX, 4
	MOV		BYTE[R8+RCX], AL
	ADD		RCX, 1
	CMP		RCX, RDX
	JB		.loop
	
.fin:
	HLT
	JMP		.fin