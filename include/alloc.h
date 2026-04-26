#ifndef ALLOC_H
#define ALLOC_H

#include "types.h"
#include "disk.h"
#include "ext2sim.h"

int load_superblock(disk_t* d,ext2sim_superblock* sb);
int save_superblock(disk_t* d,const ext2sim_superblock* sb);

u32 alloc_block(disk_t* d,ext2sim_superblock* sb);
int free_block(disk_t* d,ext2sim_superblock* sb, int block_no);

u32 alloc_inode(disk_t* d,ext2sim_superblock* sb);
int free_inode(disk_t* d, ext2sim_superblock* sb, int ino);

#endif