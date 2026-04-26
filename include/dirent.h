#ifndef DIRENT_H
#define DIRENT_H

#include "types.h"
#include "disk.h"
#include "ext2sim.h"

#define EXT2SIM_NAME_MAX 59 //leaving space for null term

typedef struct
{
    u32 inode;
    u8 name[EXT2SIM_NAME_MAX + 1];
}ext2sim_dirent;//total size=64(64 bytes so a block contains exactly 16 entries)


#define DIRENTS_PER_BLOCK (EXT2SIM_BLOCK_SIZE / (u32)sizeof(ext2sim_dirent))

u32 dirent_lookup(disk_t* d,ext2sim_superblock* sb, u32 dir_no, const char* name);
int dirent_add(disk_t* d,ext2sim_superblock* sb, u32 dir_no,const char* name, u32 ino);

#endif