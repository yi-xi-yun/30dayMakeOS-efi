#ifndef BOOTINFO_H
#define BOOTINFO_H

#include <stdint.h>
#include <stdio.h>

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

#define BOOT_INFO_MAGIC 0x424F4F54

#endif