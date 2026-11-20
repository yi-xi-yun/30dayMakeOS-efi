[BITS 64]
	ORG	0x100000

; 为C语言的运行做必要的准备工作
; 拷贝bootpack.exe至0x200000的位置，并跳转运行
_start:
    CLI
	
    MOV RSP, 0x1FF000
    MOV [BootInfoPtr], RDI      ; 暂存 UEFI 传来的 BOOT_INFO 指针

    LEA R12, [kernel_exe]       ; R12 = kernel.exe 在内存中的实际物理地址

    ; 检查PE格式
    CMP WORD [R12], 0x5A4D      ; 'MZ'
    JNE .fail
    MOV EAX, [R12 + 0x3C]       ; e_lfanew
    LEA RSI, [R12 + RAX]
    CMP DWORD [RSI], 0x00004550 ; 'PE\0\0'
    JNE .fail

    MOVZX R15, WORD [RSI + 6]   ; NumberOfSections
    MOVZX EDX, WORD [RSI + 20]  ; SizeOfOptionalHeader

    ; 获取ImageBase和EntryPoint
    MOV R8, [RSI + 24 + 24]     ; ImageBase (0x200000)
    MOV R9D, [RSI + 24 + 16]    ; AddressOfEntryPoint
    ADD R9, R8                  ; R9 = 绝对入口地址

    ; 拷贝PE Header到目标基址
    MOV EAX, [RSI + 24 + 60]    ; SizeOfHeaders (Optional Header + 0x3C)
    MOV RDI, R8
    MOV RSI, R12
    MOV RCX, RAX
    CLD
    REP MOVSB

    ; 定位节表并拷贝所有节
    MOV EAX, [R12 + 0x3C]
    LEA R11, [R12 + RAX + 24]   ; R11 = Optional Header 起始
    ADD R11, RDX                ; R11 = 第一个 Section Header

.section_loop:
    TEST R15, R15
    JZ .jump_kernel

    MOV R10D, [R11 + 0x10]      ; SizeOfRawData
    TEST R10D, R10D
    JZ .next_section            ; 如果为 0，跳过

    ; 目标 = ImageBase + VirtualAddress
    MOV EAX, [R11 + 0x0C]       ; VirtualAddress
    MOV RDI, R8
    ADD RDI, RAX

    ; 源 = kernel.exe + PointerToRawData
    MOV EAX, [R11 + 0x14]       ; PointerToRawData
    LEA RSI, [R12 + RAX]

    ; 拷贝
    MOV RCX, R10
    CLD
    REP MOVSB

.next_section:
    ADD R11, 40
    DEC R15
    JMP .section_loop

.jump_kernel:
    ; 传参并跳转
    MOV RDI, [BootInfoPtr]
    MOV RCX, RDI                ; Windows x64 ABI: 第一个参数放 RCX
    SUB RSP, 8
    JMP R9

.fail:
    JMP .fail

ALIGN 8
BootInfoPtr:
	DQ 		0
	
	
ALIGN 16
kernel_exe:
