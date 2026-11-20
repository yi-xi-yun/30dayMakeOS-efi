#ifndef BOOTINFO_H
#define BOOTINFO_H

#include <stdint.h>
#include <stdio.h>

#define COL8_000000 0x00000000
#define COL8_C6C6C6 0x00C6C6C6
#define COL8_FFFFFF 0x00FFFFFF
#define COL8_848484 0x00848484
#define COL8_008484 0x00008484

#define		ADR_IDT		0x00270000

#define		PIC0_ICW1	0x0020
#define		PIC0_OCW2	0x0020
#define		PIC0_IMR 	0x0021
#define		PIC0_ICW2	0x0021
#define		PIC0_ICW3	0x0021
#define		PIC0_ICW4	0x0021
#define		PIC1_ICW1	0x00a0
#define		PIC1_OCW2	0x00a0
#define		PIC1_IMR 	0x00a1
#define		PIC1_ICW2	0x00a1
#define		PIC1_ICW3	0x00a1
#define		PIC1_ICW4	0x00a1

typedef struct {
    uint64_t magic;
    uint64_t fb_base;
    uint64_t fb_size;
    uint32_t hres;
    uint32_t vres;
    uint32_t ppsl;
    uint32_t pixel_format;
} BOOT_INFO;

void io_hlt(void);


union InterruptDescriptorAttribute {
    uint16_t data;
    struct {
        uint16_t interrupt_stack_table : 3;
        uint16_t : 5;
        uint16_t type : 4;
        uint16_t : 1;
        uint16_t descriptor_privilege_level : 2;
        uint16_t present : 1;
    } __attribute__((packed)) bits;
} __attribute__((packed));

struct InterruptDescriptor {
    uint16_t offset_low;
    uint16_t segment_selector;
    union InterruptDescriptorAttribute attr;
    uint16_t offset_middle;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed));

struct FIFO8 {
    unsigned char *buf;
    int p;      
    int q;      
    int size;   
    int free;   
    int flags;  
};

void fifo8_init(struct FIFO8 *fifo, int size, unsigned char *buf);
int fifo8_put(struct FIFO8 *fifo, unsigned char data);
int fifo8_get(struct FIFO8 *fifo);
int fifo8_status(struct FIFO8 *fifo);

#define PORT_KEYDAT           0x0060
#define PORT_KEYSTA           0x0064
#define PORT_KEYCMD           0x0064
#define KEYSTA_SEND_NOTREADY  0x02
#define KEYCMD_WRITE_MODE     0x60
#define KBC_MODE              0x47
#define KEYCMD_SENDTO_MOUSE   0xd4
#define MOUSECMD_ENABLE       0xf4

struct MOUSE_DEC {
    unsigned char buf[3], phase;
    int x, y, btn;
};








#define BOOT_INFO_MAGIC 0x424F4F54

#endif