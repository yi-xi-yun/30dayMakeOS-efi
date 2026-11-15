// fatimg.c - FAT16 磁盘映像生成器（Windows, C99）
// 用法: fatimg.exe <输入目录> <输出.img> [大小MB, 32-2048, 默认32]
//
// 特性:
//   - MBR + 单个 FAT16 分区（类型 0xEF, EFI System Partition）
//   - 支持多文件、多子目录；子目录满自动扩容（FAT 链扩展）
//   - 8.3 短文件名（超出部分截断并警告）
//   - 按映像大小自动选择簇大小，保证落在 FAT16 合法簇数区间
//   - 连续分配，不做碎片整理
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

#define SECTOR_SIZE          512
#define RESERVED_SECTORS     1
#define NUM_FATS             2
#define ROOT_ENTRIES         512
#define ROOT_SECTORS         (ROOT_ENTRIES * 32 / SECTOR_SIZE)  // 32
#define PARTITION_START_LBA  2048

static FILE *img = NULL;
static uint16_t *fat = NULL;
static uint32_t fat_sectors = 0;
static uint32_t spc = 1;                 // sectors per cluster
static uint32_t total_sectors = 0;       // 整盘扇区
static uint32_t partition_sectors = 0;   // 分区扇区
static uint32_t data_start_sector = 0;   // 分区内数据区起点
static uint32_t cluster_count = 0;
static uint32_t next_free_cluster = 2;
static uint32_t files_written = 0;
static uint32_t dirs_written = 0;

static void die(const char *msg) {
    fprintf(stderr, "error: %s\n", msg);
    if (img) fclose(img);
    if (fat) free(fat);
    exit(1);
}

static void write_sector(uint32_t lba, const void *data) {
    if (_fseeki64(img, (long long)lba * SECTOR_SIZE, SEEK_SET) != 0) die("seek failed");
    if (fwrite(data, SECTOR_SIZE, 1, img) != 1) die("write failed");
	fflush(img);
}

static void read_sector(uint32_t lba, void *data) {
    if (_fseeki64(img, (long long)lba * SECTOR_SIZE, SEEK_SET) != 0) die("seek failed");
	fflush(img);
    if (fread(data, SECTOR_SIZE, 1, img) != 1) memset(data, 0, SECTOR_SIZE);
}

static uint32_t cluster_to_lba(uint32_t c) {
    return PARTITION_START_LBA + data_start_sector + (c - 2) * spc;
}

static uint32_t alloc_clusters(uint32_t count) {
    if (count == 0) return 0;
    if (next_free_cluster + count > cluster_count + 2) return 0;
    uint32_t start = next_free_cluster;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t c = start + i;
        fat[c] = (i == count - 1) ? 0xFFFF : (uint16_t)(c + 1);
    }
    next_free_cluster += count;
    return start;
}

static void zero_cluster(uint32_t c) {
    uint8_t zero[SECTOR_SIZE];
    memset(zero, 0, SECTOR_SIZE);
    for (uint32_t s = 0; s < spc; s++)
        write_sector(cluster_to_lba(c) + s, zero);
}

static void to_83(const char *name, uint8_t out[11], int *truncated) {
    memset(out, ' ', 11);
    *truncated = 0;
    const char *dot = strrchr(name, '.');
    int baselen = dot ? (int)(dot - name) : (int)strlen(name);
    int extlen  = dot ? (int)strlen(dot + 1) : 0;
    if (baselen > 8) { baselen = 8; *truncated = 1; }
    if (extlen  > 3) { extlen  = 3; *truncated = 1; }
    for (int i = 0; i < baselen; i++) out[i] = (uint8_t)toupper((unsigned char)name[i]);
    for (int i = 0; i < extlen;  i++) out[8 + i] = (uint8_t)toupper((unsigned char)dot[1 + i]);
}

// 在指定目录中写一个目录项。目录满且非根时自动扩容新簇。
static int write_dir_entry(uint32_t dir_first_cluster, int is_root,
                           const char *name, uint8_t attr,
                           uint32_t start_cluster, uint32_t size) {
    uint8_t sector[SECTOR_SIZE];
    uint8_t entry[32];
    memset(entry,0,32);
    
    int truncated;
    to_83(name, entry, &truncated);
    if (truncated) fprintf(stderr, "  warning: '%s' truncated to 8.3\n", name);
    entry[11] = attr;
    entry[26] = start_cluster & 0xFF;
    entry[27] = (start_cluster >> 8) & 0xFF;
    entry[28] = size & 0xFF;
    entry[29] = (size >> 8) & 0xFF;
    entry[30] = (size >> 16) & 0xFF;
    entry[31] = (size >> 24) & 0xFF;

    if (is_root) {
        uint32_t start_lba = PARTITION_START_LBA + RESERVED_SECTORS + NUM_FATS * fat_sectors;
        for (uint32_t s = 0; s < ROOT_SECTORS; s++) {
            read_sector(start_lba + s, sector);
            for (int i = 0; i < SECTOR_SIZE / 32; i++) {
                uint8_t *e = &sector[i * 32];
                if (e[0] == 0x00 || e[0] == 0xE5) {
                    memcpy(e, entry, 32);
                    write_sector(start_lba + s, sector);
                    return 0;
                }
            }
        }
        fprintf(stderr, "error: root directory full (max %d entries)\n", ROOT_ENTRIES);
        return -1;
    }

    uint32_t c = dir_first_cluster;
    for (;;) {
        uint32_t lba = cluster_to_lba(c);
        for (uint32_t s = 0; s < spc; s++) {
            read_sector(lba + s, sector);
            for (int i = 0; i < SECTOR_SIZE / 32; i++) {
                uint8_t *e = &sector[i * 32];
                if (e[0] == 0x00 || e[0] == 0xE5) {
                    memcpy(e, entry, 32);
                    write_sector(lba + s, sector);
                    return 0;
                }
            }
        }
        uint16_t next = fat[c];
        if (next >= 0xFFF8) {
            uint32_t newc = alloc_clusters(1);
            if (newc == 0) {
                fprintf(stderr, "error: no space to extend directory\n");
                return -1;
            }
            fat[c] = (uint16_t)newc;
            zero_cluster(newc);
            read_sector(cluster_to_lba(newc), sector);
            memcpy(sector, entry, 32);
            write_sector(cluster_to_lba(newc), sector);
            return 0;
        }
        c = next;
    }
}

static int process_directory(const char *host_path, uint32_t dir_first_cluster, int is_root) {
    char pattern[MAX_PATH];
    snprintf(pattern, sizeof(pattern), "%s\\*", host_path);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND) return 0;
        fprintf(stderr, "error: FindFirstFile('%s') failed (%lu)\n", host_path, err);
        return -1;
    }

    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
        char full[MAX_PATH];
        snprintf(full, sizeof(full), "%s\\%s", host_path, fd.cFileName);

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            uint32_t c = alloc_clusters(1);
            if (c == 0) {
                fprintf(stderr, "error: no space for directory '%s'\n", full);
                FindClose(h);
                return -1;
            }
            zero_cluster(c);
            uint8_t sector[SECTOR_SIZE];
            memset(sector, 0, SECTOR_SIZE);
            sector[0] = '.'; memset(sector + 1, ' ', 10);
            sector[11] = 0x10;
            sector[26] = c & 0xFF; sector[27] = (c >> 8) & 0xFF;
            sector[32] = '.'; sector[33] = '.';
            memset(sector + 34, ' ', 9);
            sector[43] = 0x10;
            uint32_t parent = is_root ? 0 : dir_first_cluster;
            sector[32 + 26] = parent & 0xFF;
            sector[32 + 27] = (parent >> 8) & 0xFF;
            write_sector(cluster_to_lba(c), sector);

            if (write_dir_entry(dir_first_cluster, is_root, fd.cFileName, 0x10, c, 0) != 0) {
                FindClose(h);
                return -1;
            }
            dirs_written++;
            if (process_directory(full, c, 0) != 0) {
                FindClose(h);
                return -1;
            }
        } else {
            FILE *f = fopen(full, "rb");
            if (!f) {
                fprintf(stderr, "error: cannot open '%s'\n", full);
                FindClose(h);
                return -1;
            }
            _fseeki64(f, 0, SEEK_END);
            long long size = _ftelli64(f);
            _fseeki64(f, 0, SEEK_SET);
            if (size < 0 || size > 0xFFFFFFFFll) {
                fprintf(stderr, "error: invalid size for '%s'\n", full);
                fclose(f);
                FindClose(h);
                return -1;
            }

            uint32_t cluster_bytes = spc * SECTOR_SIZE;
            uint32_t nclusters = (uint32_t)(((unsigned long long)size + cluster_bytes - 1) / cluster_bytes);
            uint32_t c = 0;
            if (nclusters > 0) {
                c = alloc_clusters(nclusters);
                if (c == 0) {
                    fprintf(stderr, "error: no space for file '%s' (%lld bytes)\n", full, size);
                    fclose(f);
                    FindClose(h);
                    return -1;
                }
                for (uint32_t i = 0; i < nclusters; i++) {
                    uint8_t sector[SECTOR_SIZE];
                    memset(sector, 0, SECTOR_SIZE);
                    long long pos = (long long)i * SECTOR_SIZE;
                    size_t toread = (size - pos) >= SECTOR_SIZE ? SECTOR_SIZE : (size_t)(size - pos);
                    if (fread(sector, 1, toread, f) != toread) {
                        fprintf(stderr, "error: read error on '%s'\n", full);
                        fclose(f);
                        FindClose(h);
                        return -1;
                    }
                    write_sector(cluster_to_lba(c + i), sector);
                }
            }
            fclose(f);
            if (write_dir_entry(dir_first_cluster, is_root, fd.cFileName, 0x20, c, (uint32_t)size) != 0) {
                FindClose(h);
                return -1;
            }
            files_written++;
            printf("  %-48s %10lld bytes\n", full, size);
        }
    } while (FindNextFileA(h, &fd));

    FindClose(h);
    return 0;
}

static uint32_t pick_spc(uint32_t size_mb) {
    if (size_mb <= 32)   return 1;
    if (size_mb <= 64)   return 2;
    if (size_mb <= 128)  return 4;
    if (size_mb <= 256)  return 8;
    if (size_mb <= 512)  return 16;
    if (size_mb <= 1024) return 32;
    if (size_mb <= 2048) return 64;
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 3 || argc > 4) {
        fprintf(stderr, "Usage: %s <input_dir> <output.img> [size_MB, 32-2048]\n", argv[0]);
        return 1;
    }
    uint32_t size_mb = (argc == 4) ? (uint32_t)atoi(argv[3]) : 32;
    if (size_mb < 32 || size_mb > 2048) {
        fprintf(stderr, "error: size must be 32..2048 MB\n");
        return 1;
    }
    spc = pick_spc(size_mb);
    if (spc == 0) { fprintf(stderr, "error: unsupported size\n"); return 1; }

    total_sectors     = size_mb * 1024 * 1024 / SECTOR_SIZE;
    partition_sectors = total_sectors - PARTITION_START_LBA;

    uint32_t fs = 1;
    for (;;) {
        uint32_t overhead = RESERVED_SECTORS + NUM_FATS * fs + ROOT_SECTORS;
        if (overhead >= partition_sectors) { fprintf(stderr, "error: image too small\n"); return 1; }
        uint32_t data_sectors = partition_sectors - overhead;
        uint32_t clusters = data_sectors / spc;
        uint32_t needed = ((clusters + 2) * 2 + SECTOR_SIZE - 1) / SECTOR_SIZE;
        if (needed == fs) break;
        fs = needed;
    }
    fat_sectors = fs;
    {
        uint32_t overhead = RESERVED_SECTORS + NUM_FATS * fat_sectors + ROOT_SECTORS;
        cluster_count = (partition_sectors - overhead) / spc;
        data_start_sector = overhead;
    }
    if (cluster_count < 4085 || cluster_count > 65524) {
        fprintf(stderr, "error: cluster count %u not FAT16-compatible\n", cluster_count);
        return 1;
    }

    printf("Image: %u MB (%u sectors), partition @ LBA %u (%u sectors)\n",
           size_mb, total_sectors, PARTITION_START_LBA, partition_sectors);
    printf("  FAT16: cluster=%u B, %u clusters, %u FAT sectors\n",
           spc * SECTOR_SIZE, cluster_count, fat_sectors);

    img = fopen(argv[2], "w+b");
    if (!img) { perror("fopen"); return 1; }
    if (_fseeki64(img, (long long)total_sectors * SECTOR_SIZE - 1, SEEK_SET) != 0) {
        perror("fseek"); fclose(img); return 1;
    }
    fputc(0, img);

    fat = calloc(cluster_count + 2, sizeof(uint16_t));
    if (!fat) { fclose(img); return 1; }
    fat[0] = 0xFFF8;
    fat[1] = 0xFFFF;

    // MBR
    uint8_t mbr[SECTOR_SIZE];
    memset(mbr, 0, SECTOR_SIZE);
    uint8_t *p = mbr + 0x1BE;
    p[0] = 0x00;
    p[1] = p[2] = p[3] = 0xFF;
    p[4] = 0xEF;
    p[5] = p[6] = p[7] = 0xFF;
    *(uint32_t*)(p + 8)  = PARTITION_START_LBA;
    *(uint32_t*)(p + 12) = partition_sectors;
    mbr[0x1FE] = 0x55; mbr[0x1FF] = 0xAA;
    write_sector(0, mbr);

    // FAT16 Boot Sector
    uint8_t boot[SECTOR_SIZE];
    memset(boot, 0, SECTOR_SIZE);
    boot[0] = 0xEB; boot[1] = 0x3C; boot[2] = 0x90;
    memcpy(boot + 3, "MSWIN4.1", 8);
    *(uint16_t*)(boot + 11) = SECTOR_SIZE;
    boot[13] = (uint8_t)spc;
    *(uint16_t*)(boot + 14) = RESERVED_SECTORS;
    boot[16] = NUM_FATS;
    *(uint16_t*)(boot + 17) = ROOT_ENTRIES;
    if (partition_sectors < 65536) {
        *(uint16_t*)(boot + 19) = (uint16_t)partition_sectors;
        *(uint32_t*)(boot + 32) = 0;
    } else {
        *(uint16_t*)(boot + 19) = 0;
        *(uint32_t*)(boot + 32) = partition_sectors;
    }
    boot[21] = 0xF8;
    *(uint16_t*)(boot + 22) = (uint16_t)fat_sectors;
    *(uint16_t*)(boot + 24) = 32;
    *(uint16_t*)(boot + 26) = 2;
    *(uint32_t*)(boot + 28) = PARTITION_START_LBA;
    boot[36] = 0x80;
    boot[38] = 0x29;
    *(uint32_t*)(boot + 39) = 0x12345678;
    memcpy(boot + 43, "NO NAME    ", 11);
    memcpy(boot + 54, "FAT16   ", 8);
    boot[510] = 0x55; boot[511] = 0xAA;
    write_sector(PARTITION_START_LBA, boot);

    // 清零根目录
    {
        uint8_t zero[SECTOR_SIZE];
        memset(zero, 0, SECTOR_SIZE);
        uint32_t root_start = PARTITION_START_LBA + RESERVED_SECTORS + NUM_FATS * fat_sectors;
        for (uint32_t i = 0; i < ROOT_SECTORS; i++)
            write_sector(root_start + i, zero);
    }

    printf("Populating from '%s' ...\n", argv[1]);
    if (process_directory(argv[1], 0, 1) != 0) {
        fclose(img); free(fat); return 1;
    }

    // 写两份 FAT
    for (int f = 0; f < NUM_FATS; f++) {
        uint32_t start = PARTITION_START_LBA + RESERVED_SECTORS + f * fat_sectors;
        for (uint32_t s = 0; s < fat_sectors; s++) {
            uint8_t sector[SECTOR_SIZE];
            memset(sector, 0, SECTOR_SIZE);
            for (int i = 0; i < SECTOR_SIZE / 2; i++) {
                uint32_t idx = s * (SECTOR_SIZE / 2) + i;
                if (idx < cluster_count + 2)
                    *(uint16_t*)(sector + i * 2) = fat[idx];
            }
            write_sector(start + s, sector);
        }
    }

    uint32_t used = next_free_cluster - 2;
    printf("\nOK: %u files, %u directories, %u/%u clusters used (%.1f%%)\n",
           files_written, dirs_written, used, cluster_count,
           100.0 * used / cluster_count);
    printf("Image: %s\n", argv[2]);

    free(fat);
    fclose(img);
    return 0;
}
