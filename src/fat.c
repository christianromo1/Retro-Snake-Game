#define SECTOR_SIZE 512
#include <stdint.h>
#include "fat.h"
#include "ide.h"

// Screen output from kernel_main.c
extern void puts(const char *str);
extern void putc(int c);

// ============ GLOBALS ============

static char boot_buf[SECTOR_SIZE];
static char fat_buf[64 * SECTOR_SIZE];
static char rde_buf[32 * SECTOR_SIZE];

static struct boot_sector *bs;
static uint32_t root_sector;
static uint32_t root_dir_sectors;
static uint32_t data_start;

// ============ INTERNAL HELPERS ============

static int strcmp_k(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

static char toupper_k(char c) {
    if (c >= 'a' && c <= 'z') return c - 32;
    return c;
}

static void extract_filename(struct root_directory_entry *rde, char *fname) {
    int k = 0;

    while (k < 8 && rde->file_name[k] != ' ') {
        fname[k] = rde->file_name[k];
        k++;
    }
    fname[k] = '\0';

    if (rde->file_extension[0] == ' ')
        return;

    fname[k++] = '.';

    int n = 0;
    while (n < 3 && rde->file_extension[n] != ' ') {
        fname[k++] = rde->file_extension[n];
        n++;
    }
    fname[k] = '\0';
}

// ============ fatInit ============

int fatInit(void) {
    const uint32_t partition_base = 2048;

    ata_lba_read(partition_base, (unsigned char *)boot_buf, 1);
    bs = (struct boot_sector *)boot_buf;

    if (bs->boot_signature != 0xAA55) {
        puts("fatInit: ERROR - bad boot signature\n");
        return -1;
    }

    {
        char *ft = bs->fs_type;
        if (!(ft[0] == 'F' && ft[1] == 'A' && ft[2] == 'T' &&
              ft[3] == '1' && ft[4] == '6')) {
            puts("fatInit: ERROR - not FAT16\n");
            return -1;
        }
    }

    if (bs->bytes_per_sector != 512) {
        puts("fatInit: ERROR - unsupported sector size\n");
        return -1;
    }

    {
        uint32_t fat_lba = partition_base + bs->num_reserved_sectors;
        ata_lba_read(fat_lba, (unsigned char *)fat_buf, bs->num_sectors_per_fat);
    }

    root_sector = partition_base
                + bs->num_reserved_sectors
                + (uint32_t)bs->num_fat_tables * bs->num_sectors_per_fat;

    root_dir_sectors =
        ((uint32_t)bs->num_root_dir_entries * 32 + SECTOR_SIZE - 1) / SECTOR_SIZE;

    data_start = root_sector + root_dir_sectors;

    return 0;
}

// ============ fatOpen ============

static struct file open_file;

struct file *fatOpen(const char *filename) {
    ata_lba_read(root_sector, (unsigned char *)rde_buf, root_dir_sectors);

    struct root_directory_entry *rde = (struct root_directory_entry *)rde_buf;

    char upper[13];
    int j = 0;
    while (filename[j] && j < 12) {
        upper[j] = toupper_k(filename[j]);
        j++;
    }
    upper[j] = '\0';

    for (uint32_t i = 0; i < bs->num_root_dir_entries; i++) {
        unsigned char first = (unsigned char)rde[i].file_name[0];

        if (first == 0x00) break;
        if (first == 0xE5) continue;
        if (rde[i].attribute == 0x0F) continue;

        char fname[13];
        extract_filename(&rde[i], fname);

        if (strcmp_k(fname, upper) == 0) {
            open_file.next = (struct file *)0;
            open_file.prev = (struct file *)0;
            open_file.rde = rde[i];
            open_file.start_cluster = rde[i].cluster;
            return &open_file;
        }
    }

    puts("fatOpen: file not found\n");
    return (struct file *)0;
}

// ============ fatRead ============

static char cluster_buf[CLUSTER_SIZE];

int fatRead(struct file *f, char *buf, uint32_t nbytes) {
    if (!f) return -1;

    uint32_t cluster = f->start_cluster;
    uint32_t bytes_read = 0;
    uint32_t file_size = f->rde.file_size;
    uint32_t spc = bs->num_sectors_per_cluster;
    uint32_t bytes_per_cluster = spc * SECTOR_SIZE;
    uint16_t *fat16 = (uint16_t *)fat_buf;

    if (nbytes > file_size)
        nbytes = file_size;

    while (bytes_read < nbytes && cluster >= 2 && cluster < 0xFFF8) {
        uint32_t lba = data_start + (cluster - 2) * spc;
        ata_lba_read(lba, (unsigned char *)cluster_buf, spc);

        uint32_t to_copy = bytes_per_cluster;
        if (bytes_read + to_copy > nbytes)
            to_copy = nbytes - bytes_read;

        for (uint32_t k = 0; k < to_copy; k++)
            buf[bytes_read + k] = cluster_buf[k];

        bytes_read += to_copy;
        cluster = fat16[cluster];
    }

    return (int)bytes_read;
}
