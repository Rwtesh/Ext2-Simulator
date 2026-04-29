#include "../include/ls.h"
#include "../include/disk.h"
#include "../include/alloc.h"
#include "../include/inode.h"
#include "../include/path.h"
#include "../include/dirent.h"

#include <stdio.h>
#include <string.h>

static int isDir(const ext2sim_inode* in)
{
    return ((in->mode & EXT2SIM_MT) == EXT2SIM_DIR);    //only the first 4 bits tell us the mode
    //rest tell us teh permissions which we dc about 
}

int cmd_ls(const char* imgPath, const char* absPath)
{
    if (!imgPath) return -1;
    if (!absPath || absPath[0] == '\0') absPath = "/";

    disk_t d;
    if (disk_open(&d, imgPath, "rb", EXT2SIM_BLOCK_SIZE) != 0) {
        fprintf(stderr, "ls failed to open image\n");
        return -1;
    }

    ext2sim_superblock sb;
    if (load_superblock(&d, &sb) != 0) {
        fprintf(stderr, "ls failed to load superblock\n");
        disk_close(&d);
        return -1;
    }

    u32 ino = path_resolve(&d, &sb, absPath);
    if (ino == 0) {
        fprintf(stderr, "ls path not found: %s\n", absPath);
        disk_close(&d);
        return -1;
    }

    ext2sim_inode dir;
    if (inode_read(&d, &sb, ino, &dir) != 0) {
        fprintf(stderr, "ls inode_read failed\n");
        disk_close(&d);
        return -1;
    }

    if (!isDir(&dir)) {
        fprintf(stderr, "ls not a directory: %s\n", absPath);
        disk_close(&d);
        return -1;
    }

    u32 blocksUsed = (dir.size + sb.block_size - 1) / sb.block_size;
    if (blocksUsed > 12) blocksUsed = 12;

    u8 buf[EXT2SIM_BLOCK_SIZE];

    for (u32 bi = 0; bi < blocksUsed; bi++) {
        u32 blk = dir.direct[bi];
        if (blk == 0) continue;

        if (disk_read_block(&d, blk, buf) != 0) {
            fprintf(stderr, "ls failed to read block %u\n", blk);
            disk_close(&d);
            return -1;
        }

        ext2sim_dirent* ents = (ext2sim_dirent*)buf;
        for (u32 ei = 0; ei < DIRENTS_PER_BLOCK; ei++) {
            if (ents[ei].inode == 0) continue;

            ents[ei].name[EXT2SIM_NAME_MAX] = '\0';

            if (strcmp((char*)ents[ei].name, ".") == 0) continue;
            if (strcmp((char*)ents[ei].name, "..") == 0) continue;

            printf("%s\n", ents[ei].name);
        }
    }

    disk_close(&d);
    return 0;
}