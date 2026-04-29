#include "../include/touch.h"
#include "../include/disk.h"
#include "../include/alloc.h"
#include "../include/inode.h"
#include "../include/dirent.h"
#include "../include/path.h"

#include <string.h>
#include <stdio.h>

static int endWithSlash(const char* s)
{
    size_t n = s ? strlen(s) : 0;
    return (n > 1 && s[n - 1] == '/'); // / alone handled separately to avoid dumb bugs
}

int cmdTouch(const char* imgPath, const char* absPath)
{
    if (!imgPath || !absPath) return -1;

    if (strcmp(absPath, "/") == 0) {
        fprintf(stderr, "touch invalid path '/'\n");
        return -1;
    }
    if (absPath[0] != '/') {
        fprintf(stderr, "touch only absolute paths supported\n");
        return -1;
    }
    if (endWithSlash(absPath)) {
        fprintf(stderr, "touch path ends with '/', not a file\n");
        return -1;
    }

    disk_t d;
    if (disk_open(&d, imgPath, "r+b", EXT2SIM_BLOCK_SIZE) != 0) {
        fprintf(stderr, "touch failed to open image\n");
        return -1;
    }

    ext2sim_superblock sb;
    if (load_superblock(&d, &sb) != 0) {
        fprintf(stderr, "touch failed to load superblock\n");
        disk_close(&d);
        return -1;
    }

    u32 parent_ino = 0;
    char leaf[EXT2SIM_NAME_MAX + 1];

    if (path_parent(&d, &sb, absPath, &parent_ino, leaf) != 0) {
        fprintf(stderr, "touch invalid path or parent not found\n");
        disk_close(&d);
        return -1;
    }

    if (strcmp(leaf, ".") == 0 || strcmp(leaf, "..") == 0) {
        fprintf(stderr, "touch: invalid name '%s'\n", leaf);
        disk_close(&d);
        return -1;
    }

    // if already exists succeed since its alright
    u32 existing = dirent_lookup(&d, &sb, parent_ino, leaf);
    if (existing != 0) {
        disk_close(&d);
        return 0;
    }

    u32 ino = alloc_inode(&d, &sb);
    if (ino == 0) {
        fprintf(stderr, "touch no free inodes\n");
        disk_close(&d);
        return -1;
    }

    ext2sim_inode f;
    memset(&f, 0, sizeof(f));
    f.mode = (u16)(EXT2SIM_REG | 0644);
    f.links_count = 1;
    f.size = 0;
    if (inode_write(&d, &sb, ino, &f) != 0) {
        fprintf(stderr, "touch inode_write failed\n");
        free_inode(&d, &sb, (int)ino);
        disk_close(&d);
        return -1;
    }

    if (dirent_add(&d, &sb, parent_ino, leaf, ino) != 0) {
        fprintf(stderr, "touch failed to add directory entry\n");
        free_inode(&d, &sb, (int)ino);
        disk_close(&d);
        return -1;
    }
    save_superblock(&d, &sb);

    disk_close(&d);
    return 0;
}