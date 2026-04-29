#include "../include/rm.h"
#include "../include/disk.h"
#include "../include/alloc.h"
#include "../include/inode.h"
#include "../include/dirent.h"
#include "../include/path.h"
#include "../include/ext2sim.h"

#include <stdio.h>
#include <string.h>

int test(int x)
{
    // printf("Hello %d\n",x);
    return x;
}

static int endsWithSlash(const char* s)
{
    size_t n = s ? strlen(s) : 0;
    return (n > 1 && s[n - 1] == '/');
}

static int isDir(const ext2sim_inode* in)
{
    return ((in->mode & EXT2SIM_MT) == EXT2SIM_DIR);
}

int cmd_rm(const char* imgPath, const char* absPath)
{
    if (!imgPath || !absPath) return -1;
    test(1);
    if (strcmp(absPath, "/") == 0) {
        fprintf(stderr, "rm cannot remove '/'\n");
        return -1;
    }
    if (absPath[0] != '/') {
        fprintf(stderr, "rm only absolute paths supported\n");
        return -1;
    }
    if (endsWithSlash(absPath)) {
        fprintf(stderr, "rm path ends with '/'\n");
        return -1;
    }

    disk_t d;
    if (disk_open(&d, imgPath, "r+b", EXT2SIM_BLOCK_SIZE) != 0) {
        fprintf(stderr, "rm failed to open image\n");
        return -1;
    }

    ext2sim_superblock sb;
    if (load_superblock(&d, &sb) != 0) {
        fprintf(stderr, "rm failed to load superblock\n");
        disk_close(&d);
        return -1;
    }

    u32 parent = 0;
    char leaf[EXT2SIM_NAME_MAX + 1];

    if (path_parent(&d, &sb, absPath, &parent, leaf) != 0) {
        fprintf(stderr, "rm invalid path or parent not found\n");
        disk_close(&d);
        return -1;
    }

    if (strcmp(leaf, ".") == 0 || strcmp(leaf, "..") == 0) {
        fprintf(stderr, "rm refusing to remove '%s'\n", leaf);
        disk_close(&d);
        return -1;
    }

    u32 ino = dirent_lookup(&d, &sb, parent, leaf);
    if (ino == 0) {
        fprintf(stderr, "rm not found: %s\n", absPath);
        disk_close(&d);
        return -1;
    }

    ext2sim_inode v;
    if (inode_read(&d, &sb, ino, &v) != 0) {
        fprintf(stderr, "rm inode_read failed\n");
        disk_close(&d);
        return -1;
    }
    test(2);
    if (isDir(&v)) {
        fprintf(stderr, "rm is a directory (rmdir not implemented)\n");
        disk_close(&d);
        return -1;
    }

    // remove name first 
    if (dirent_remove(&d, &sb, parent, leaf) != 0) {
        fprintf(stderr, "rm failed to remove directory entry\n");
        disk_close(&d);
        return -1;
    }

    // free direct blocks
    for (u32 i = 0; i < 12; i++) {
        if (v.direct[i] != 0) {
            if (free_block(&d, &sb, (int)v.direct[i]) != 0) {
                fprintf(stderr, "rm warning failed to free block %u\n", v.direct[i]);
                // continue freeing others
            }
            v.direct[i] = 0;
        }
    }

    if (free_inode(&d, &sb, (int)ino) != 0) {
        fprintf(stderr, "rm warning failed to free inode %u\n", ino);
        // The name is already gone worst case = leaked an inode
    }

    save_superblock(&d, &sb);
    disk_close(&d);
    return 0;
}