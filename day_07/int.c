#include "bootpack.h"

void init_pic(void) {
  io_out8(PIC0_IMR, 0xff);
  io_out8(PIC1_IMR, 0xff);

  io_out8(PIC0_ICW1, 0x11);
  io_out8(PIC0_ICW2, 0x20);
  io_out8(PIC0_ICW3, 1 << 2);
  io_out8(PIC0_ICW4, 0x01);

  io_out8(PIC1_ICW1, 0x11);
  io_out8(PIC1_ICW2, 0x28);
  io_out8(PIC1_ICW3, 2);
  io_out8(PIC1_ICW4, 0x01);
  
  io_out8(PIC0_IMR, 0xfd);
  io_out8(PIC1_IMR, 0xff);
}

extern BOOT_INFO *g_bootinfo;

extern struct FIFO8 keyfifo, mousefifo;

__attribute__((interrupt)) void inthandler21(int *esp) {
	unsigned char data=io_in8(0x60);
	fifo8_put(&keyfifo, data);
	
	io_out8(PIC0_OCW2, 0x61);
	return;
}

__attribute__((interrupt)) void inthandler2c(int *esp) {
    unsigned char data = io_in8(PORT_KEYDAT);
    fifo8_put(&mousefifo, data);

    io_out8(PIC1_OCW2,0x64); /* 通知PIC1（从PIC） IRQ-12的受理已经完成 */
	io_out8(PIC0_OCW2,0x62); /* 通知PIC0（主PIC） IRQ-02的受理已经完成 */
}

__attribute__((interrupt)) void inthandler27(int *esp) {
	io_out8(PIC0_OCW2, 0x67);

	return;
}

union InterruptDescriptorAttribute make_idt_attr(uint16_t type, uint16_t dpl) {
    union InterruptDescriptorAttribute attr;
    attr.data = 0;
    attr.bits.type = type;
    attr.bits.descriptor_privilege_level = dpl;
    attr.bits.present = 1;
    return attr;
}

void set_idt_entry(struct InterruptDescriptor *desc, union InterruptDescriptorAttribute attr, uint64_t offset, uint16_t segment_selector) {
    desc->attr = attr;
    desc->offset_low = offset & 0xffffu;
    desc->offset_middle = (offset >> 16) & 0xffffu;
    desc->offset_high = offset >> 32;
    desc->segment_selector = segment_selector;
}

void init_idt(void) {
    struct InterruptDescriptor *idt = (struct InterruptDescriptor *) ADR_IDT;

    for (int i = 0; i < 256; i++) {
        set_idt_entry(&idt[i], make_idt_attr(0, 0), 0, 0);
    }

    uint16_t cs = get_cs();
	/*char s[40];
    sprintf(s, "CS..%d", cs);
    put_fonts32_asc(g_bootinfo->fb_base, g_bootinfo->ppsl, 16, 64, COL8_FFFFFF, s);
	
	uint16_t ds=get_ds(),ss=get_ss();
	sprintf(s, "DS..%d SS..%d",ds,ss);
	put_fonts32_asc(g_bootinfo->fb_base, g_bootinfo->ppsl, 16, 88, COL8_FFFFFF, s);*/

    set_idt_entry(&idt[0x21], make_idt_attr(14, 0), (uint64_t)inthandler21, cs);
	set_idt_entry(&idt[0x27], make_idt_attr(14, 0), (uint64_t)inthandler27, cs);
    set_idt_entry(&idt[0x2c], make_idt_attr(14, 0), (uint64_t)inthandler2c, cs);
	
    load_idtr(256 * 16 - 1, ADR_IDT);
    init_pic();
}