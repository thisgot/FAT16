#ifndef FAT16_H
#define FAT16_H

#include <stdint.h>
#include <stdio.h>

#define DIR_FREE_ENTRY 0xE5

#define DIR_ATTR_READONLY 1 << 0  /* file is read only */
#define DIR_ATTR_HIDDEN 1 << 1    /* file is hidden */
#define DIR_ATTR_SYSTEM 1 << 2    /* system file (also hidden) */
#define DIR_ATTR_VOLUMEID 1 << 3  /* special entry containing disk volume label */
#define DIR_ATTR_DIRECTORY 1 << 4 /* describes a subdirectory */
#define DIR_ATTR_ARCHIVE 1 << 5   /* archive flag (always set when file is modified */
#define DIR_ATTR_LFN 0xf          /* not used */

#define SIG 0xAA55 /* boot sector signature -- sector is executable */

#pragma pack(push, 1)

/* FAT Directory Entry */
struct fat_dir
{
    unsigned char name[11];    /* Short name + file extension */
    uint8_t attr;              /* file attributes */
    uint8_t ntres;             /* reserved for Windows NT, Set value to 0 when a file is created */
    uint8_t creation_stamp;    /* millisecond timestamp at file creation time */
    uint16_t creation_time;    /* time file was created */
    uint16_t creation_date;    /* date file was created */
    uint16_t last_access_date; /* last access date (last read/written) */
    uint16_t reserved_fat32;   /* reserved for fat32 */
    uint16_t last_write_time;  /* time of last write */
    uint16_t last_write_date;  /* date of last write */
    uint16_t starting_cluster; /* starting cluster */
    uint32_t file_size;        /* size of the file in bytes */
};

/* Boot Sector and BPB (BIOS Parameter Block)
 * Located at the first sector of the volume in the reserved region.
 * Known as the boot sector, reserved sector, or the "0th" sector.
 */
struct fat_bpb
{                              /* BIOS Parameter Block */
    uint8_t jump_code[3];      /* code to jump to the bootstrap code */
    unsigned char oem_name[8]; /* Oem ID: name of the formatting OS */

    uint16_t bytes_p_sect;      /* bytes per sector */
    uint8_t sector_p_clust;     /* sectors per cluster */
    uint16_t reserved_sect;     /* reserved sectors */
    uint8_t n_fat;              /* number of FAT copies */
    uint16_t possible_rentries; /* number of possible root entries */
    uint16_t snumber_sect;      /* small number of sectors */

    uint8_t media_desc;       /* media descriptor */
    uint16_t sect_per_fat16;  /* sectors per FAT (FAT16 only) */
    uint16_t sect_per_track;  /* sectors per track */
    uint16_t number_of_heads; /* number of heads */
    uint32_t hidden_sects;    /* hidden sectors */
    uint32_t large_n_sects;   /* large number of sectors */

    /* FAT32-specific fields */
    uint32_t sect_per_fat32;     /* sectors per FAT (FAT32 only) */
    uint16_t ext_flags;          /* extended flags */
    uint16_t fs_version;         /* file system version */
    uint32_t root_cluster;       /* cluster where the root directory starts */
    uint16_t fsinfo_sector;      /* sector number of FSInfo structure */
    uint16_t backup_boot_sector; /* sector number of backup boot sector */
    uint8_t reserved[12];        /* reserved for future use */
};

#pragma pack(pop)

/* Prototypes for reading and handling FAT structures */
int read_bytes(FILE *, unsigned int, void *, unsigned int);
void rfat(FILE *, struct fat_bpb *);

/* Prototypes for calculating FAT-specific addresses */
uint32_t bpb_faddress(struct fat_bpb *);            // Address of the FAT
uint32_t bpb_froot_addr(struct fat_bpb *);          // Address of root directory
uint32_t bpb_fdata_addr(struct fat_bpb *);          // Address of data region
uint32_t bpb_fdata_sector_count(struct fat_bpb *);  // Number of sectors in data region
uint32_t bpb_fdata_cluster_count(struct fat_bpb *); // Number of clusters in data region

/// Additional Definitions
#define FAT16STR_SIZE 11
#define FAT16STR_SIZE_WNULL 12

#define RB_ERROR -1
#define RB_OK 0

/* FAT16 and FAT32 EOF markers */
#define FAT16_EOF_LO 0xfff8
#define FAT16_EOF_HI 0xffff
#define FAT32_EOF 0x0FFFFFFF

#endif /* FAT16_H */
