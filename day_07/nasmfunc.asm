[BITS 64]
[SECTION .text]
	GLOBAL	io_hlt, io_cli, io_sti, io_stihlt, io_out8, io_in8, load_idtr, get_cs, get_ds, get_ss

io_hlt:
    HLT
    RET

io_cli:
    CLI
    RET

io_sti:
    STI
    RET

io_stihlt:
    STI
    HLT
    RET

io_out8:
    MOV		AL, DL
	MOV		DX, CX
    OUT		DX, AL
    RET
	
io_in8:
	MOV		DX, CX
	XOR		EAX, EAX
	IN		AL, DX
	RET

load_idtr:
    SUB RSP, 10
    MOV [RSP], CX
    MOV [RSP+2], RDX
    LIDT [RSP]
    ADD RSP, 10
    RET
	
get_cs:
    XOR EAX, EAX
    MOV AX, CS
    RET
	
get_ds:
	XOR	EAX, EAX
	MOV	AX, DS
	RET

get_ss:
	XOR	EAX, EAX
	MOV	AX, SS
	RET

