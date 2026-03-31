#ifndef INODE_H
#define INODE_H

#include "types.h"
#include "ext2sim.h"
#include "disk.h"

int inode_read(disk_t* d,const ext2sim_superblock* sb, u32 ino, ext2sim_inode* out);
int inode_write(disk_t* d, const ext2sim_superblock* sb, u32 ino, const ext2sim_inode* in);

#endif