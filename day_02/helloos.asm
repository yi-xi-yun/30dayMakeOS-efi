; bootx64.asm —— 纯汇编 UEFI 引导加载器骨架
; 目标：读取 \kernel.bin → 拷贝到 0x100000 → ExitBootServices
;       → 跳转到 0x100000

[BITS 64]
[DEFAULT REL]

; ---------- 常量 ----------
EFI_SUCCESS             EQU		0
EFI_LOADER_DATA         EQU		2
EFI_FILE_MODE_READ      EQU		1
KERNEL_LOAD_ADDR        EQU		0x100000

; ---------- BOOT_INFO 字段偏移 ----------
BI_MAGIC                EQU		0x00
BI_FB_BASE              EQU		0x08
BI_FB_SIZE              EQU		0x10
BI_FB_HRES              EQU		0x18
BI_FB_VRES              EQU		0x1C
BI_FB_PPSL              EQU		0x20
BI_FB_PIXEL_FORMAT      EQU		0x24
BI_FB_RED_MASK          EQU		0x28
BI_FB_GREEN_MASK        EQU		0x2C
BI_FB_BLUE_MASK         EQU		0x30
BI_MEMMAP_PHYS          EQU		0x38
BI_MEMMAP_SIZE          EQU		0x40
BI_MEMMAP_DESC_SIZE     EQU		0x48
BI_MEMMAP_DESC_VER      EQU		0x50



[SECTION .text]
GLOBAL _EfiMain

; UEFI 入口
;   RCX = EFI_HANDLE        ImageHandle
;   RDX = EFI_SYSTEM_TABLE* SystemTable
_EfiMain:
    ; 保存固件传入的两个参数
    MOV [ImageHandle], RCX
    MOV [SystemTable], RDX

    ; 栈：入口时 RSP % 16 == 8（CALL 压了返回地址）
    ; PUSH RBP → RSP % 16 == 0；SUB 0x20 → 仍 % 16 == 0
    ; 这样后续每次 CALL 前 RSP % 16 == 0，符合 MS x64 约定
    PUSH RBP
    MOV  RBP, RSP
    SUB  RSP, 0x20          ; 32 字节 shadow space

    ; 获取 EFI_LOADED_IMAGE_PROTOCOL
    CALL Stage1_GetLoadedImage
    TEST RAX, RAX
    JZ   .fail
    LEA  RDX, [gMsgStage1]
    CALL PrintString

    ; 获取 EFI_SIMPLE_FILE_SYSTEM_PROTOCOL
    CALL Stage2_GetSimpleFileSystem
    TEST RAX, RAX
    JZ   .fail
    LEA  RDX, [gMsgStage2]
    CALL PrintString

    ; 把 \kernel.bin 读进内存
    CALL Stage3_ReadKernelFile
    TEST RAX, RAX
    JZ   .fail
    LEA  RDX, [gMsgStage3]
    CALL PrintString

    CALL Stage3_5_VerifyKernel
    TEST RAX, RAX
    JZ   .fail
    LEA  RDX, [gMsgStage3_5]
    CALL PrintString

    ; 拷贝到 KERNEL_LOAD_ADDR
    CALL Stage4_CopyKernel
    TEST RAX, RAX
    JZ   .fail

    ; GetMemoryMap ×2 + ExitBootServices
    CALL Stage5_ExitBootServices
    TEST RAX, RAX           ; 返回 0 表示成功，非 0 表示失败
    JNZ  .fail

    ; 移交控制权（永不返回）
    JMP  Stage6_JumpToKernel

.fail:
    CALL PrintFailMessage
.halt:
    CLI
    HLT
    JMP .halt


 
; Stage1_GetLoadedImage
;   用 HandleProtocol 从 ImageHandle 获取 EFI_LOADED_IMAGE_PROTOCOL
;   输入：ImageHandle / SystemTable（全局）
;   输出：RAX = EFI_LOADED_IMAGE_PROTOCOL*，失败返回 0
;   副作用：LoadedImage 被设置
 
Stage1_GetLoadedImage:
    PUSH RBP
    MOV  RBP, RSP
    SUB  RSP, 0x20

    ; --- 从 SystemTable 取 BootServices->HandleProtocol ---
    MOV  RAX, [SystemTable]
    TEST RAX, RAX
    JZ   .fail
    MOV  RAX, [RAX + 0x60]           ; EFI_SYSTEM_TABLE.BootServices
    TEST RAX, RAX
    JZ   .fail
    MOV  RAX, [RAX + 0x98]           ; EFI_BOOT_SERVICES.HandleProtocol
    TEST RAX, RAX
    JZ   .fail

    ; --- HandleProtocol(ImageHandle, &Guid, &LoadedImage) ---
    MOV  RCX, [ImageHandle]
    LEA  RDX, [gEfiLoadedImageProtocolGuid]
    LEA  R8,  [LoadedImage]
    CALL RAX

    ; --- 检查 EFI_STATUS（EFI_SUCCESS == 0） ---
    TEST RAX, RAX
    JNZ  .fail

    MOV  RAX, [LoadedImage]
    TEST RAX, RAX
    JZ   .fail

    LEAVE
    RET

.fail:
    XOR  RAX, RAX
    LEAVE
    RET


 
; Stage2_GetSimpleFileSystem
;   用 HandleProtocol 从 LoadedImage->DeviceHandle 获取
;   EFI_SIMPLE_FILE_SYSTEM_PROTOCOL
;   输入：LoadedImage / SystemTable（全局）
;   输出：RAX = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL*，失败返回 0
;   副作用：SimpleFileSystem 被设置
 
Stage2_GetSimpleFileSystem:
    PUSH RBP
    MOV  RBP, RSP
    SUB  RSP, 0x20

    ; --- 从 LoadedImage 取 DeviceHandle ---
    MOV  RAX, [LoadedImage]
    TEST RAX, RAX
    JZ   .fail
    MOV  RCX, [RAX + 0x18]           ; EFI_LOADED_IMAGE_PROTOCOL.DeviceHandle
    TEST RCX, RCX
    JZ   .fail

    ; --- 从 SystemTable 取 BootServices->HandleProtocol ---
    MOV  RAX, [SystemTable]
    TEST RAX, RAX
    JZ   .fail
    MOV  RAX, [RAX + 0x60]           ; EFI_SYSTEM_TABLE.BootServices
    TEST RAX, RAX
    JZ   .fail
    MOV  RAX, [RAX + 0x98]           ; EFI_BOOT_SERVICES.HandleProtocol
    TEST RAX, RAX
    JZ   .fail

    ; --- HandleProtocol(DeviceHandle, &Guid, &SimpleFileSystem) ---
    LEA  RDX, [gEfiSimpleFileSystemProtocolGuid]
    LEA  R8,  [SimpleFileSystem]
    CALL RAX

    ; --- 检查 EFI_STATUS ---
    TEST RAX, RAX
    JNZ  .fail

    MOV  RAX, [SimpleFileSystem]
    TEST RAX, RAX
    JZ   .fail

    LEAVE
    RET

.fail:
    XOR  RAX, RAX
    LEAVE
    RET

 

 
; Stage3_ReadKernelFile
;   读取 \kernel.bin 到 KernelBuffer，大小写入 KernelSize
;   输出：RAX = 1 成功 / 0 失败
 
Stage3_ReadKernelFile:
    PUSH RBP
    MOV  RBP, RSP
    SUB  RSP, 0x20

    CALL Stage3_OpenRootVolume
    TEST RAX, RAX
    JZ   .fail

    CALL Stage3_OpenKernelFile
    TEST RAX, RAX
    JZ   .fail

    CALL Stage3_GetKernelFileSize
    TEST RAX, RAX
    JZ   .fail

    CALL Stage3_AllocateAndReadKernel
    TEST RAX, RAX
    JZ   .fail

    ; 关闭文件（失败也不影响已读入的缓冲区，但仍建议关闭）
    MOV  RCX, [KernelFile]
    TEST RCX, RCX
    JZ   .done
    CALL QWORD [RCX + 0x10]          ; EFI_FILE_PROTOCOL.Close

.done:
    MOV  RAX, 1
    LEAVE
    RET
.fail:
    XOR  RAX, RAX
    LEAVE
    RET

 
; Stage3_OpenRootVolume
;   SimpleFileSystem->OpenVolume(&Root)
;   输出：RAX = 1 成功 / 0 失败
;   副作用：Root 被设置
 
Stage3_OpenRootVolume:
    PUSH RBP
    MOV  RBP, RSP
    SUB  RSP, 0x20

    MOV  RCX, [SimpleFileSystem]
    TEST RCX, RCX
    JZ   .fail
    LEA  RDX, [Root]
    CALL QWORD [RCX + 0x08]          ; EFI_SIMPLE_FILE_SYSTEM_PROTOCOL.OpenVolume

    TEST RAX, RAX
    JNZ  .fail
    MOV  RAX, [Root]
    TEST RAX, RAX
    JZ   .fail

    MOV  RAX, 1
    LEAVE
    RET
.fail:
    XOR  RAX, RAX
    LEAVE
    RET

 
; Stage3_OpenKernelFile
;   Root->Open(&KernelFile, L"\\kernel.bin",
;              EFI_FILE_MODE_READ, 0)
;   输出：RAX = 1 成功 / 0 失败
;   副作用：KernelFile 被设置
 
Stage3_OpenKernelFile:
    PUSH RBP
    MOV  RBP, RSP
    SUB  RSP, 0x30

    MOV  RCX, [Root]
    TEST RCX, RCX
    JZ   .fail
    LEA  RDX, [KernelFile]           ; **NewHandle
    LEA  R8,  [gKernelPath]          ; *FileName (CHAR16*)
    MOV  R9,  1                      ; EFI_FILE_MODE_READ
    ; 第 5 个参数 Attributes = 0 走栈
    MOV  QWORD [RSP + 0x20], 0
    CALL QWORD [RCX + 0x08]          ; EFI_FILE_PROTOCOL.Open

    TEST RAX, RAX
    JNZ  .fail
    MOV  RAX, [KernelFile]
    TEST RAX, RAX
    JZ   .fail

    MOV  RAX, 1
    LEAVE
    RET
.fail:
    XOR  RAX, RAX
    LEAVE
    RET


 
; Stage3_GetKernelFileSize
;   KernelFile->GetInfo(EFI_FILE_INFO_ID, &Size, NULL) 两次
;   第一次拿所需大小，第二次拿 EFI_FILE_INFO
;   输出：RAX = 1 成功 / 0 失败
;   副作用：KernelSize 被设置；FileInfoBuffer 被分配
 
Stage3_GetKernelFileSize:
    PUSH RBP
    MOV  RBP, RSP
    SUB  RSP, 0x20

    ; --- 第一次 GetInfo：Size = 0, Buffer = NULL ---
    MOV  RCX, [KernelFile]
    TEST RCX, RCX
    JZ   .fail
    LEA  RDX, [gEfiFileInfoGuid]     ; InformationType
    LEA  R8,  [FileInfoSize]         ; *BufferSize
    MOV  QWORD [FileInfoSize], 0
    XOR  R9,  R9                     ; Buffer = NULL
    CALL QWORD [RCX + 0x40]          ; EFI_FILE_PROTOCOL.GetInfo

    ; 期望 EFI_BUFFER_TOO_SMALL，但任何非零都是“拿到了大小”
    ; 如果返回 EFI_SUCCESS 且 Size 为 0，说明文件为空，提前失败
    CMP  QWORD [FileInfoSize], 0
    JE   .fail

    ; --- AllocatePool(EfiLoaderData, FileInfoSize, &FileInfoBuffer) ---
    MOV  RAX, [SystemTable]
    MOV  RAX, [RAX + 0x60]           ; BootServices
    MOV  RAX, [RAX + 0x40]           ; AllocatePool
    MOV  RCX, 2                      ; EfiLoaderData
    MOV  RDX, [FileInfoSize]
    LEA  R8,  [FileInfoBuffer]
    CALL RAX
    TEST RAX, RAX
    JNZ  .fail

    ; --- 第二次 GetInfo：拿真正的 EFI_FILE_INFO ---
    MOV  RCX, [KernelFile]
    LEA  RDX, [gEfiFileInfoGuid]
    LEA  R8,  [FileInfoSize]
    MOV  R9,  [FileInfoBuffer]
    CALL QWORD [RCX + 0x40]          ; GetInfo

    TEST RAX, RAX
    JNZ  .fail

    ; --- 从 EFI_FILE_INFO + 0x08 取 FileSize ---
    MOV  RAX, [FileInfoBuffer]
    MOV  RAX, [RAX + 0x08]           ; EFI_FILE_INFO.FileSize
    MOV  [KernelSize], RAX
    TEST RAX, RAX
    JZ   .fail                       ; 空文件视为失败

    MOV  RAX, 1
    LEAVE
    RET
.fail:
    XOR  RAX, RAX
    LEAVE
    RET


 
; Stage3_AllocateAndReadKernel
;   AllocatePool(KernelSize, &KernelBuffer)
;   KernelFile->Read(&ReadSize, KernelBuffer)
;   输出：RAX = 1 成功 / 0 失败
;   副作用：KernelBuffer 被设置；KernelSize 已是文件大小
 
Stage3_AllocateAndReadKernel:
    PUSH RBP
    MOV  RBP, RSP
    SUB  RSP, 0x20

    ; --- AllocatePool(EfiLoaderData, KernelSize, &KernelBuffer) ---
    MOV  RAX, [SystemTable]
    MOV  RAX, [RAX + 0x60]
    MOV  RAX, [RAX + 0x40]           ; AllocatePool
    MOV  RCX, 2                      ; EfiLoaderData
    MOV  RDX, [KernelSize]
    LEA  R8,  [KernelBuffer]
    CALL RAX
    TEST RAX, RAX
    JNZ  .fail

    ; --- Read(&ReadSize, KernelBuffer) ---
    MOV  RCX, [KernelFile]
    TEST RCX, RCX
    JZ   .fail
    LEA  RDX, [ReadSize]
    MOV  RAX, [KernelSize]
    MOV  QWORD [ReadSize], RAX       ; 期望读满
    MOV  R8,  [KernelBuffer]
    CALL QWORD [RCX + 0x20]          ; EFI_FILE_PROTOCOL.Read

    TEST RAX, RAX
    JNZ  .fail

    ; --- 确认实际读到的字节数 == KernelSize ---
    MOV  RAX, [ReadSize]
    CMP  RAX, [KernelSize]
    JNE  .fail

    MOV  RAX, 1
    LEAVE
    RET
.fail:
    XOR  RAX, RAX
    LEAVE
    RET

 
; Stage3_5_VerifyKernel
;   校验 KernelBuffer 前 3 字节 == F4 EB FD
;   输出：RAX = 1 成功 / 0 失败
 
Stage3_5_VerifyKernel:
    PUSH RBP
    MOV  RBP, RSP
    SUB  RSP, 0x20

    MOV  RAX, [KernelBuffer]
    TEST RAX, RAX
    JZ   .fail

    MOV  RCX, [KernelSize]
    CMP  RCX, 3
    JB   .fail
	
	CMP  BYTE [RAX+0], 0xf4
    JNE  .fail
    CMP  BYTE [RAX+1], 0xeb
    JNE  .fail
    CMP  BYTE [RAX+2], 0xfd
    JNE  .fail

    MOV  RAX, 1
    LEAVE
    RET
.fail:
    MOV  RAX, [KernelBuffer]
    TEST RAX, RAX
    JZ   .really_fail
    MOV  RCX, [KernelSize]
    TEST RCX, RCX
    JZ   .really_fail

    LEA  RDX, [gMsgStage3_5Fail]
    CALL PrintString

    MOV  RAX, [KernelBuffer]
    MOVZX EAX, BYTE [RAX + 0]
    CALL PrintHexByte
    LEA  RDX, [gMsgSpace]
    CALL PrintString

    MOV  RAX, [KernelSize]
    CMP  RAX, 2
    JB   .really_fail
    MOV  RAX, [KernelBuffer]
    MOVZX EAX, BYTE [RAX + 1]
    CALL PrintHexByte
    LEA  RDX, [gMsgSpace]
    CALL PrintString

    MOV  RAX, [KernelSize]
    CMP  RAX, 3
    JB   .really_fail
    MOV  RAX, [KernelBuffer]
    MOVZX EAX, BYTE [RAX + 2]
    CALL PrintHexByte
    LEA  RDX, [gMsgCrLf]
    CALL PrintString

.really_fail:
    XOR  RAX, RAX
    LEAVE
    RET


 
; Stage4_CopyKernel
;   输入：全局 KernelBuffer / KernelSize
;   输出：RAX = 1 成功 / 0 失败
;   副作用：内核字节被拷贝到 KERNEL_LOAD_ADDR
 
Stage4_CopyKernel:
    PUSH RBP
    MOV  RBP, RSP
    SUB  RSP, 0x20

    MOV  RSI, [KernelBuffer]
    MOV  RCX, [KernelSize]
    TEST RSI, RSI
    JZ   .copy_fail
    TEST RCX, RCX
    JZ   .copy_fail

    ; TODO（建议）：用 GetMemoryMap 确认 KERNEL_LOAD_ADDR
    ;   落在 EfiConventionalMemory 描述符内，且长度 >= KernelSize。
    ;   第一个版本可以先跳过，但在真实机器上不是可选项。

    MOV  RDI, KERNEL_LOAD_ADDR
    CLD                     ; 确保 DF=0
    REP  MOVSB

    MOV  RAX, 1
    LEAVE
    RET
.copy_fail:
    XOR  RAX, RAX
    LEAVE
    RET


 
; Stage5_ExitBootServices
;   GetMemoryMap ×2 → ExitBootServices
;   输出：RAX = 0 成功（Boot Services 已失效，不能打印）
;         RAX = 1 失败（Boot Services 仍有效，可以打印）
 
Stage5_ExitBootServices:
    PUSH RBP
    MOV  RBP, RSP
    SUB  RSP, 0x30            ; 32 字节 shadow + 第 5 个栈参数

    ; 检查 SystemTable
    MOV  RAX, [SystemTable]
    TEST RAX, RAX
    JZ   .fail
    MOV  RAX, [RAX + 0x60]    ; BootServices
    TEST RAX, RAX
    JZ   .fail

    ; ---------- 第一次 GetMemoryMap ----------
    ; GetMemoryMap(&Size, NULL, &Key, &DescSize, &DescVer)
    MOV  QWORD [MemoryMapSize], 0
    LEA  RCX, [MemoryMapSize]
    XOR  RDX, RDX
    LEA  R8,  [MapKey]
    LEA  R9,  [DescriptorSize]
    LEA  R10, [DescriptorVersion]
    MOV  [RSP + 0x20], R10     ; 第 5 个参数走栈
    MOV  RAX, [SystemTable]
    MOV  RAX, [RAX + 0x60]
    MOV  RAX, [RAX + 0x38]     ; EFI_BOOT_SERVICES.GetMemoryMap
    CALL RAX

    ; 第一次返回 EFI_BUFFER_TOO_SMALL 是正常的；只检查 Size 是否被填上
    CMP  QWORD [MemoryMapSize], 0
    JE   .fail

    ; MemoryMapSize += DescriptorSize，留一个描述符的余量
    MOV  RAX, [MemoryMapSize]
    ADD  RAX, [DescriptorSize]
    MOV  [MemoryMapSize], RAX

    ; ---------- AllocatePool 分配内存映射缓冲区 ----------
    ; ⚠ 这一步必须在第二次 GetMemoryMap 之前，
    ;    因为 AllocatePool 会改变内存映射，会使 MapKey 过期
    MOV  RAX, [SystemTable]
    MOV  RAX, [RAX + 0x60]
    MOV  RAX, [RAX + 0x40]     ; EFI_BOOT_SERVICES.AllocatePool
    MOV  RCX, 2                ; EfiLoaderData
    MOV  RDX, [MemoryMapSize]
    LEA  R8,  [MemoryMapBuffer]
    CALL RAX
    TEST RAX, RAX
    JNZ  .fail
    MOV  RAX, [MemoryMapBuffer]
    TEST RAX, RAX
    JZ   .fail

    ; ---------- 第二次 GetMemoryMap ----------
    ; 从这里到 ExitBootServices 之间不能有任何改变内存映射的调用
    LEA  RCX, [MemoryMapSize]
    MOV  RDX, [MemoryMapBuffer]
    LEA  R8,  [MapKey]
    LEA  R9,  [DescriptorSize]
    LEA  R10, [DescriptorVersion]
    MOV  [RSP + 0x20], R10
    MOV  RAX, [SystemTable]
    MOV  RAX, [RAX + 0x60]
    MOV  RAX, [RAX + 0x38]
    CALL RAX
    TEST RAX, RAX
    JNZ  .fail                 ; 第一版不重试；返回非零直接失败

    ; 把内存映射的物理地址、大小、描述符大小写进 BOOT_INFO
    MOV  RDI, [BootInfo]
    TEST RDI, RDI
    JZ   .skip_memmap
    MOV  RAX, [MemoryMapBuffer]
    MOV  [RDI + BI_MEMMAP_PHYS], RAX
    MOV  RAX, [MemoryMapSize]
    MOV  [RDI + BI_MEMMAP_SIZE], RAX
    MOV  RAX, [DescriptorSize]
    MOV  [RDI + BI_MEMMAP_DESC_SIZE], RAX
    MOV  EAX, [DescriptorVersion]
    MOV  [RDI + BI_MEMMAP_DESC_VER], EAX
.skip_memmap:

    ; ---------- ExitBootServices(ImageHandle, MapKey) ----------
    MOV  RAX, [SystemTable]
    MOV  RAX, [RAX + 0x60]
    MOV  RAX, [RAX + 0xE8]     ; EFI_BOOT_SERVICES.ExitBootServices
    MOV  RCX, [ImageHandle]
    MOV  RDX, [MapKey]
    CALL RAX

    ; 非零 = 失败；此时 Boot Services 仍有效，可以打印错误
    TEST RAX, RAX
    JNZ  .fail

    ; 成功：Boot Services 已失效，不能再调用 PrintString
    XOR  RAX, RAX
    LEAVE
    RET

.fail:
    MOV  RAX, 1
    LEAVE
    RET


 
; Stage6_JumpToKernel
;   永不返回。执行到这里时 Boot Services 已失效。
 
Stage6_JumpToKernel:
    ; TODO（可选，但推荐）：
    ;   - MOV CR3, [KernelPageTable]   ; 如果内核要求自有页表
    ;   - MOV RSP, [KernelStackTop]    ; 建立内核栈
    ;   - MOV AX, 0
    ;     MOV DS, AX / MOV ES, AX / MOV SS, AX
    ;   - 按约定把启动参数放到寄存器或栈上

    CLI
    MOV  RAX, KERNEL_LOAD_ADDR
    JMP  RAX


 
; PrintFailMessage
;   用 ConOut->OutputString 输出 gFailMsg。
;   依赖：SystemTable（全局，_EfiMain 入口已保存）
;   破坏：RAX, RCX, RDX, R8-R11（均为 caller-saved）
 
PrintFailMessage:
    PUSH RBP
    MOV  RBP, RSP
    SUB  RSP, 0x20

    MOV  RCX, [SystemTable]
    TEST RCX, RCX
    JZ   .done                       ; SystemTable 未初始化则直接返回

    MOV  RCX, [RCX + 0x40]           ; EFI_SYSTEM_TABLE.ConOut
    TEST RCX, RCX
    JZ   .done

    LEA  RDX, [gFailMsg]             ; CHAR16*
    CALL QWORD [RCX + 0x08]          ; SIMPLE_TEXT_OUTPUT_INTERFACE.OutputString

.done:
    LEAVE
    RET

 
; PrintString
;   输入：RDX = CHAR16* 字符串（NULL 结尾）
;   依赖：SystemTable（全局）
;   破坏：RAX, RCX, RDX, R8-R11
 
PrintString:
    PUSH RBP
    MOV  RBP, RSP
    SUB  RSP, 0x20

    MOV  RCX, [SystemTable]
    TEST RCX, RCX
    JZ   .done
    MOV  RCX, [RCX + 0x40]           ; ConOut
    TEST RCX, RCX
    JZ   .done
    CALL QWORD [RCX + 0x08]          ; OutputString(RCX, RDX)

.done:
    LEAVE
    RET

 
; PrintHexByte
;   输入：AL = 要打印的字节
;   破坏：RAX, RCX, RDX, R8-R11
 
PrintHexByte:
    PUSH RBP
    MOV  RBP, RSP
    SUB  RSP, 0x20
    PUSH RBX                       ; 保存 RBX（callee-saved）
    SUB  RSP, 0x10                 ; 对齐

    MOVZX EBX, AL
    MOV  EAX, EBX
    SHR  EAX, 4
    AND  EAX, 0x0F
    LEA  RDX, [gHexChars]
    MOVZX EAX, BYTE [RDX + RAX]
    MOV  [gHexBuf + 0], AX

    MOV  EAX, EBX
    AND  EAX, 0x0F
    MOVZX EAX, BYTE [RDX + RAX]
    MOV  [gHexBuf + 2], AX
    MOV  WORD [gHexBuf + 4], 0

    LEA  RDX, [gHexBuf]
    CALL PrintString

    ADD  RSP, 0x10
    POP  RBX
    LEAVE
    RET


 
; 数据段
 
[SECTION .data]
ALIGN 8

ImageHandle       DQ 0
SystemTable       DQ 0
LoadedImage       DQ 0
SimpleFileSystem  DQ 0
KernelBuffer      DQ 0
KernelSize        DQ 0
MemoryMapBuffer   DQ 0
MemoryMapSize     DQ 0
MapKey            DQ 0
DescriptorSize    DQ 0
DescriptorVersion DD 0

; ---------- GUID ----------
ALIGN 16
gEfiLoadedImageProtocolGuid:
    ; {5B1B31A1-9562-11d2-8E3F-00A0C969723B}
    DD 0x5B1B31A1
    DW 0x9562
    DW 0x11d2
    DB 0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B

gEfiSimpleFileSystemProtocolGuid:
    ; {964E5B22-6459-11d2-8E39-00A0C969723B}
    DD 0x964E5B22
    DW 0x6459
    DW 0x11d2
    DB 0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B

; ---------- 字符串 ----------
ALIGN 8
gFailMsg:
    DW __utf16__("Boot failed."), 13, 10, 0
gMsgStage1:
    DW __utf16__("Stage 1 OK"), 13, 10, 0
gMsgStage2:
    DW __utf16__("Stage 2 OK"), 13, 10, 0

ALIGN 8
Root             DQ 0
KernelFile       DQ 0
FileInfoBuffer   DQ 0
FileInfoSize     DQ 0
ReadSize         DQ 0

ALIGN 16
gEfiFileInfoGuid:
    ; {9576E92-6D3F-11D2-8E39-00A0C969723B}
    DD 0x09576E92
    DW 0x6D3F
    DW 0x11D2
    DB 0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B

gKernelPath:
    DW __utf16__("\KERNEL.BIN"), 0
gMsgStage3:
    DW __utf16__("Stage 3 OK"), 13, 10, 0
gMsgStage3_5:
    DW __utf16le__("F4 EB FD OK"), 13, 10, 0
gHexChars:        DB "0123456789ABCDEF", 0
gHexBuf:          DW 0, 0, 0           ; 2 个字符 + 终止符
gMsgStage3_5Fail: DW __utf16le__("Stage 3.5 FAIL, bytes: "), 0
gMsgSpace:        DW __utf16le__(" "), 0
gMsgCrLf:         DW __utf16le__(""), 13, 10, 0

; ---------- 全局变量 ----------
Gop              DQ 0
GopMode          DQ 0
GopInfo          DQ 0
BootInfo         DQ 0

; ---------- GOP GUID ----------
ALIGN 16
gEfiGraphicsOutputProtocolGuid:
    ; {9042A9DE-23DC-4A38-96FB-7ADED080516A}
    DD 0x9042A9DE
    DW 0x23DC
    DW 0x4A38
    DB 0x96, 0xFB, 0x7A, 0xDE, 0xD0, 0x80, 0x51, 0x6A

; ---------- 调试输出 ----------
gMsgStage4:
    DW __utf16le__("Stage 4 OK"), 13, 10, 0