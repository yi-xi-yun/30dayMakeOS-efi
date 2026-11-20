#include "bootpack.h"

void box_fill32(uint32_t *vram, int ppsl, uint32_t color, int x0, int y0, int x1, int y1) {
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            vram[y * ppsl + x] = color;
        }
    }
}

void put_font32(uint32_t *vram, int ppsl, int x, int y, uint32_t color, const unsigned char *font) {
    for (int i = 0; i < 16; i++) {
        unsigned char d = font[i];
        uint32_t *p = vram + (y + i) * ppsl + x;
        
        if ((d & 0x80) != 0) p[0] = color;
        if ((d & 0x40) != 0) p[1] = color;
        if ((d & 0x20) != 0) p[2] = color;
        if ((d & 0x10) != 0) p[3] = color;
        if ((d & 0x08) != 0) p[4] = color;
        if ((d & 0x04) != 0) p[5] = color;
        if ((d & 0x02) != 0) p[6] = color;
        if ((d & 0x01) != 0) p[7] = color;
    }
}


extern const char _binary_hankaku_bin_start[];

void put_fonts32_asc(uint32_t *vram, int ppsl, int x, int y, uint32_t color, const char *s) {
    
    for (; *s != '\0'; s++) {
        put_font32(vram, ppsl, x, y, color, _binary_hankaku_bin_start + (unsigned char)(*s) * 16);
        x += 8;
    }
}

void init_screen(uint32_t *vram, int ppsl, int xsize, int ysize) {
    box_fill32(vram, ppsl, COL8_008484, 0, 0, xsize - 1, ysize - 29);
    box_fill32(vram, ppsl, COL8_C6C6C6, 0, ysize - 28, xsize - 1, ysize - 28);
    box_fill32(vram, ppsl, COL8_FFFFFF, 0, ysize - 27, xsize - 1, ysize - 27);
    box_fill32(vram, ppsl, COL8_C6C6C6, 0, ysize - 26, xsize - 1, ysize - 1);

    box_fill32(vram, ppsl, COL8_FFFFFF, 3, ysize - 24, 59, ysize - 24);
    box_fill32(vram, ppsl, COL8_FFFFFF, 2, ysize - 24, 2, ysize - 4);
    box_fill32(vram, ppsl, COL8_848484, 3, ysize - 4, 59, ysize - 4);
    box_fill32(vram, ppsl, COL8_848484, 59, ysize - 23, 59, ysize - 5);
    box_fill32(vram, ppsl, COL8_000000, 2, ysize - 4, 59, ysize - 3);
    box_fill32(vram, ppsl, COL8_000000, 60, ysize - 24, 60, ysize - 3);

    box_fill32(vram, ppsl, COL8_848484, xsize - 47, ysize - 24, xsize - 4, ysize - 24);
    box_fill32(vram, ppsl, COL8_848484, xsize - 47, ysize - 23, xsize - 47, ysize - 4);
    box_fill32(vram, ppsl, COL8_FFFFFF, xsize - 47, ysize - 3, xsize - 4, ysize - 3);
    box_fill32(vram, ppsl, COL8_FFFFFF, xsize - 3, ysize - 24, xsize - 3, ysize - 3);
}

BOOT_INFO *g_bootinfo;
struct FIFO8 keyfifo, mousefifo;

//等待键盘控制电路准备完毕
void wait_KBC_sendready(void) {
    for (;;) {
        if ((io_in8(PORT_KEYSTA) & KEYSTA_SEND_NOTREADY) == 0) {
            break;
        }
    }
}

//初始化键盘控制电路（同时激活鼠标电路）
void init_keyboard(void) {
    wait_KBC_sendready();
    io_out8(PORT_KEYCMD, KEYCMD_WRITE_MODE);
    wait_KBC_sendready();
    io_out8(PORT_KEYDAT, KBC_MODE);
}

// 激活鼠标本身
void enable_mouse(void) {
    wait_KBC_sendready();
    io_out8(PORT_KEYCMD, KEYCMD_SENDTO_MOUSE);
    wait_KBC_sendready();
    io_out8(PORT_KEYDAT, MOUSECMD_ENABLE);     //0xF4是激活鼠标指令，鼠标收到后会回0xFA
}

int mouse_decode(struct MOUSE_DEC *mdec, unsigned char dat) {
    if (mdec->phase == 0) {
        if (dat == 0xFA) {
            mdec->phase = 1;
        }
        return 0;
    }
    if (mdec->phase == 1) {
        if ((dat & 0xC8) == 0x08) {
            mdec->buf[0] = dat;
            mdec->phase = 2;
        }
        return 0;
    }
    if (mdec->phase == 2) {
        mdec->buf[1] = dat;
        mdec->phase = 3;
        return 0;
    }
    if (mdec->phase == 3) {
        mdec->buf[2] = dat;
        mdec->phase = 1;

        mdec->btn = mdec->buf[0] & 0x07;
        mdec->x = mdec->buf[1];
        mdec->y = mdec->buf[2];

        if ((mdec->buf[0] & 0x10) != 0) {
            mdec->x |= 0xFFFFFF00;
        }
        if ((mdec->buf[0] & 0x20) != 0) {
            mdec->y |= 0xFFFFFF00;
        }

        mdec->y = -mdec->y; 
        return 1;
    }
    return -1;
}

void init_mouse_cursor32(uint32_t *mouse, uint32_t bc) {
    static char cursor[16][16] = {
        "**************..",
        "*OOOOOOOOOOO*...",
        "*OOOOOOOOOO*....",
        "*OOOOOOOOO*.....",
        "*OOOOOOOO*......",
        "*OOOOOOO*.......",
        "*OOOOOOO*.......",
        "*OOOOOOOO*......",
        "*OOOO**OOO*.....",
        "*OOO*..*OOO*....",
        "*OO*....*OOO*...",
        "*O*......*OOO*..",
        "**........*OOO*.",
        "*..........*OOO*",
        "............*OO*",
        ".............***"
    };

    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            if (cursor[y][x] == '*') {
                mouse[y * 16 + x] = 0x00000000; // 黑色边框
            }
            if (cursor[y][x] == 'O') {
                mouse[y * 16 + x] = 0x00FFFFFF; // 白色填充
            }
            if (cursor[y][x] == '.') {
                mouse[y * 16 + x] = bc;         // 背景色（透明）
            }
        }
    }
}

// 32位色块绘制 (用于把鼠标画在屏幕上)
void put_block32(uint32_t *vram, int ppsl, int pxsize, int pysize,
                 int px0, int py0, uint32_t *buf, int bxsize) {
    for (int y = 0; y < pysize; y++) {
        for (int x = 0; x < pxsize; x++) {
            vram[(py0 + y) * ppsl + (px0 + x)] = buf[y * bxsize + x];
        }
    }
}

void kernel_main(BOOT_INFO *boot_info) {
    if (boot_info->magic != BOOT_INFO_MAGIC) {
        goto fail;
    }

	g_bootinfo=(uint32_t *)boot_info;

    uint32_t *vram = (uint32_t *)boot_info->fb_base;
    int xsize = boot_info->hres;
    int ysize = boot_info->vres;
    int ppsl = boot_info->ppsl;

	init_screen(vram,ppsl,xsize,ysize);

    //put_fonts32_asc(vram, ppsl, 8, 8, COL8_FFFFFF, "ABC 123");
    //put_fonts32_asc(vram, ppsl, 31, 31, COL8_000000, "Haribote OS.");
    //put_fonts32_asc(vram, ppsl, 30, 30, COL8_FFFFFF, "Haribote OS.");
	
    static unsigned char keybuf[32];
    static unsigned char mousebuf[128];
    fifo8_init(&keyfifo, 32, keybuf);
    fifo8_init(&mousefifo, 128, mousebuf);
	
	init_idt();

	io_out8(PIC0_IMR, 0xf9); // 开放PIC1以及键盘中断
	io_out8(PIC1_IMR, 0xef); // 开放鼠标中断

	init_keyboard();
	enable_mouse();
	
    char s[40];
    unsigned char i;
    struct MOUSE_DEC mdec;


    int mx = (boot_info->hres - 16) / 2; 
    int my = (boot_info->vres - 16) / 2;
    uint32_t mcursor[256];

    init_mouse_cursor32(mcursor, COL8_008484);
    
    put_block32((uint32_t *)boot_info->fb_base, boot_info->ppsl, 16, 16, mx, my, mcursor, 16);

    for (;;) {
        io_cli();
        if (fifo8_status(&keyfifo) + fifo8_status(&mousefifo) == 0) {
            io_stihlt();
        } else {
            if (fifo8_status(&keyfifo) != 0) {
                i = fifo8_get(&keyfifo);
                io_sti();
				
				sprintf(s, "%02X", i);
                box_fill32((uint32_t *)g_bootinfo->fb_base, g_bootinfo->ppsl, COL8_008484, 0, 16, 15, 31);
                put_fonts32_asc((uint32_t *)g_bootinfo->fb_base, g_bootinfo->ppsl, 0, 16, COL8_FFFFFF, s);
            } 
            else if (fifo8_status(&mousefifo) != 0) {
                i = fifo8_get(&mousefifo);
                io_sti();
                
                if (mouse_decode(&mdec, i) != 0) {
                    sprintf(s, "[lcr %4d %4d]", mdec.x, mdec.y);
                    if ((mdec.btn & 0x01) != 0) s[1] = 'L';
                    if ((mdec.btn & 0x02) != 0) s[3] = 'R';
                    if ((mdec.btn & 0x04) != 0) s[2] = 'C';
                    
                    box_fill32((uint32_t *)boot_info->fb_base, boot_info->ppsl, COL8_008484, 32, 16, 32 + 15 * 8 - 1, 31);
                    put_fonts32_asc((uint32_t *)boot_info->fb_base, boot_info->ppsl, 32, 16, COL8_FFFFFF, s);

                    box_fill32((uint32_t *)boot_info->fb_base, boot_info->ppsl, COL8_008484, mx, my, mx + 15, my + 15);
                    
                    mx += mdec.x;
                    my += mdec.y;

                    if (mx < 0) mx = 0;
                    if (my < 0) my = 0;
                    if (mx > boot_info->hres - 16) mx = boot_info->hres - 16;
                    if (my > boot_info->vres - 16) my = boot_info->vres - 16;

                    sprintf(s, "(%3d, %3d)", mx, my);
                    box_fill32((uint32_t *)boot_info->fb_base, boot_info->ppsl, COL8_008484, 0, 0, 79, 15);
                    put_fonts32_asc((uint32_t *)boot_info->fb_base, boot_info->ppsl, 0, 0, COL8_FFFFFF, s);
                    
                    put_block32((uint32_t *)boot_info->fb_base, boot_info->ppsl, 16, 16, mx, my, mcursor, 16);
                }
            }
        }
    }

	
fail:
    for(;;){
		io_hlt();
	}
}