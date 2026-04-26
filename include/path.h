#ifndef PATH_H
#define PATH_H

#include "types.h"
#include "disk.h"
#include "ext2sim.h"
#include "dirent.h"

// resolve an absolute path to an ino
u32 path_resolve(disk_t* d, ext2sim_superblock* sb, const char* abs_path);

// return the final component name in leaf_out
int path_parent(disk_t* d,ext2sim_superblock* sb,const char* abs_path,char leaf_out[EXT2SIM_NAME_MAX + 1]);

#endif