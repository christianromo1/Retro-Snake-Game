//fat.c
#define SECTOR_SIZE 512
#include <stdint.h>
#include "fat.h"
#include "ide.h"

// Screen output from kernel_main.c
extern void puts(const char *str);
extern void putc(int c);

// ============ GLOBALS ============

static char boot_buf[SECTOR_SIZE];               // holds raw boot sector
static char fat_buf[64 * SECTOR_SIZE];           // holds FAT table (up to 32KB)
static char rde_buf[32 * SECTOR_SIZE];           // holds root directory region

static struct boot_sector *bs;                   // points into boot_buf
static uint32_t root_sector;                     // absolute LBA of root dir
static uint32_t root_dir_sectors;                // how many sectors the root dir takes
static uint32_t data_start;                      // absolute LBA of first data cluster

// ============ INTERNAL HELPERS ============

static int strcmp_k(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

static char toupper_k(char c) {
    if (c >= 'a' && c <= 'z') return c - 32;
    return c;
}

// Lifted from professor's fstest.c — extracts "FILENAME.EXT" from an RDE.
static void extract_filename(struct root_directory_entry *rde, char *fname) {
    int k = 0;
    while (k < 8 && rde->file_name[k] != ' ') {
        fname[k] = rde->file_name[k];
        k++;
    }
    fname[k] = '\0';

    if (rde->file_extension[0] == ' ') return;

    fname[k++] = '.';
    int n = 0;
    while (n < 3 && rde->file_extension[n] != ' ') {
        fname[k++] = rde->file_extension[n++];
    }
    fname[k] = '\0';
}

// ============ fatInit ============
// Reads the FAT16 boot sector and FAT table from disk into memory.
// Computes root_sector and data_start for use by fatOpen/fatRead.




// ============ fatOpen ============
// Searches the root directory for a file matching `filename`.
// Returns a pointer to a struct file on success, NULL if not found.
// FAT stores filenames in uppercase — convert before comparing.

static struct file open_file;   // one open file at a time (static storage)

struct file *fatOpen(const char *filename) {
    // Read entire root directory region into rde_buf
    ata_lba_read(root_sector, (unsigned char *)rde_buf, root_dir_sectors);

    struct root_directory_entry *rde = (struct root_directory_entry *)rde_buf;

    // Build uppercase version of the search name
    char upper[13];
    int j = 0;
    while (filename[j] && j < 12) {
        upper[j] = toupper_k(filename[j]);
        j++;
    }
    upper[j] = '\0';

    // Walk every RDE in the root directory
    for (uint32_t i = 0; i < bs->num_root_dir_entries; i++) {
        unsigned char first = (unsigned char)rde[i].file_name[0];
        if (first == 0x00) break;   // 0x00 = no more entries after this
        if (first == 0xE5) continue; // 0xE5 = deleted entry, skip
	if (rde[i].attribute == 0x0F) continue;

        char fname[13];
        extract_filename(&rde[i], fname);

        if (strcmp_k(fname, upper) == 0) {
            open_file.next          = (struct file *)0;
            open_file.prev          = (struct file *)0;
            open_file.rde           = rde[i];
            open_file.start_cluster = rde[i].cluster;
            return &open_file;
        }
    }

    puts("fatOpen: file not found\n");
    return (struct file *)0;
}

// ============ fatRead ============
// Reads up to `nbytes` of data from file `f` into `buf`.
// Follows the FAT16 cluster chain until the file ends or nbytes are read.
// Returns number of bytes actually read.

static char cluster_buf[CLUSTER_SIZE];  // one cluster at a time (4096 bytes)

int fatRead(struct file *f, char *buf, uint32_t nbytes) {
    if (!f) return -1;

    uint32_t cluster           = f->start_cluster;
    uint32_t bytes_read        = 0;
    uint32_t file_size         = f->rde.file_size;
    uint32_t spc               = bs->num_sectors_per_cluster;
    uint32_t bytes_per_cluster = spc * SECTOR_SIZE;
    uint16_t *fat16            = (uint16_t *)fat_buf;

    // Don't read more than the file actually contains
    if (nbytes > file_size) nbytes = file_size;

    // Walk the cluster chain (FAT16 end-of-chain marker >= 0xFFF8)
    while (bytes_read < nbytes && cluster >= 2 && cluster < 0xFFF8) {
        // Convert cluster number to absolute LBA.
        // Cluster 2 is the first data cluster (clusters 0 and 1 are reserved).
        uint32_t lba = data_start + (cluster - 2) * spc;
        ata_lba_read(lba, (unsigned char *)cluster_buf, spc);

        uint32_t to_copy = bytes_per_cluster;
        if (bytes_read + to_copy > nbytes)
            to_copy = nbytes - bytes_read;

        for (uint32_t k = 0; k < to_copy; k++)
            buf[bytes_read + k] = cluster_buf[k];

        bytes_read += to_copy;

        // Follow the FAT chain: fat16[cluster] gives the next cluster number
        cluster = fat16[cluster];
    }

    return (int)bytes_read;
}
