#ifndef MKFS_H
#define MKFS_H

#include<stdint.h>
#include "types.h"

int cmd_mkfs(const char* img_path,u32 size_mb);

#endif