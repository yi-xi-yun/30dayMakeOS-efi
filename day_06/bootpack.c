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

	init_idt();
	io_sti();
		
fail:
    for(;;){
		io_hlt();
	}
}