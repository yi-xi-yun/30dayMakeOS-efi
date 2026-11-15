NASM		= nasm
LD			= ld
FATIMG		= ..\fatimg\fatimg.exe
VBOX_MANAGE = "C:\Program Files\Oracle\VirtualBox\VBoxManage.exe"

VM_NAME     = helloos
STORAGECTL  = IDE

all: efi.vdi

helloos.obj: helloos.asm
	$(NASM) -f win64 -o helloos.obj helloos.asm

helloos.efi: helloos.obj
	$(LD) -m i386pep -pie --subsystem=10 --entry=_EfiMain -o helloos.efi helloos.obj
	
KERNEL.BIN: kernel.asm
	$(NASM) -f bin -o kernel.bin kernel.asm

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
	@-del /f /q *.obj *.efi efi.img efi.vdi kernel.bin 2>nul
	@-rmdir /s /q efi 2>nul

.PHONY: all clean