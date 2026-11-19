NASM		= nasm
LD			= ld
FATIMG		= ..\fatimg\fatimg.exe
VBOX_MANAGE = "C:\Program Files\Oracle\VirtualBox\VBoxManage.exe"

VM_NAME     = helloos
STORAGECTL  = IDE

CC			= gcc
CFLAGS 		= -c -ffreestanding -fno-builtin -fno-stack-protector -mno-red-zone -fno-pie -fno-asynchronous-unwind-tables -O2
OBJCOPY     = objcopy

all: efi.vdi

helloos.obj: helloos.asm
	$(NASM) -f win64 -o helloos.obj helloos.asm

helloos.efi: helloos.obj
	$(LD) -m i386pep -pie --subsystem=10 --entry=_EfiMain -o helloos.efi helloos.obj
	
#新增
bootpack.obj: bootpack.c bootpack.h
	$(CC) $(CFLAGS) -o bootpack.obj bootpack.c

kernel.obj: kernel.asm
	$(NASM) -f win64 -o kernel.obj kernel.asm -l kernel.lst
	
nasmfunc.obj: nasmfunc.asm
	$(NASM) -f win64 -o nasmfunc.obj nasmfunc.asm -l nasmfunc.lst

vsprintf.obj: ..\libc\vsprintf.c
	$(CC) -c ..\libc\vsprintf.c -o vsprintf.obj
	
hankaku.obj: hankaku.bin
	$(OBJCOPY) -I binary -O pe-x86-64 -B i386:x86-64 hankaku.bin hankaku.obj

kernel.exe: bootpack.obj kernel.obj nasmfunc.obj vsprintf.obj hankaku.obj kernel.ld
	$(LD) -m i386pep -T kernel.ld -o kernel.exe bootpack.obj kernel.obj nasmfunc.obj vsprintf.obj hankaku.obj

KERNEL.BIN: kernel.exe
	$(OBJCOPY) -O binary -j .text -j .rdata kernel.exe kernel.bin

#到这里结束
	
	


efi/KERNEL.BIN: KERNEL.BIN
	@if not exist "efi" mkdir "efi"
	@copy /Y KERNEL.BIN efi\KERNEL.BIN

efi/EFI/BOOT/BOOTX64.EFI: helloos.efi
	@if not exist "efi\EFI\BOOT" mkdir "efi\EFI\BOOT"
	@copy /Y helloos.efi efi\EFI\BOOT\BOOTX64.EFI

efi.img: efi/EFI/BOOT/BOOTX64.EFI efi/KERNEL.BIN
	$(FATIMG) efi efi.img

efi.vdi: efi.img
	@-$(VBOX_MANAGE) storageattach $(VM_NAME) --storagectl $(STORAGECTL) --port 0 --device 0 --medium none
	@-$(VBOX_MANAGE) closemedium disk efi.vdi
	@-del /f /q efi.vdi 2>nul
	@$(VBOX_MANAGE) convertfromraw efi.img efi.vdi --format VDI
	@$(VBOX_MANAGE) storageattach $(VM_NAME) --storagectl $(STORAGECTL) --port 0 --device 0 --type hdd --medium efi.vdi

clean:
	@-del /f /q *.obj *.lst *.efi efi.img efi.vdi kernel.exe kernel.bin 2>nul
	@-rmdir /s /q efi 2>nul

.PHONY: all clean