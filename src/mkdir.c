#include "../include/mkdir.h"
#include "../include/disk.h"
#include "../include/alloc.h"
#include "../include/inode.h"
#include "../include/dirent.h"
#include "../include/path.h"
#include "../include/ext2sim.h"

#include <stdio.h>
#include <string.h>

int jsfortest(int yo)
{
    // printf("Here rn %d \n",yo);
    return yo;
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

int cmd_mkdir(const char* imgPath, const char* absPath)
{
    jsfortest(1);
    if (!imgPath || !absPath) return -1;

    if (strcmp(absPath, "/") == 0) {
        fprintf(stderr, "mkdir: cannot create '/'\n");
        return -1;
    }
    if (absPath[0] != '/') {
        fprintf(stderr, "mkdir: only absolute paths supported\n");
        return -1;
    }
    if (endsWithSlash(absPath)) {
        fprintf(stderr, "mkdir: path ends with '/'\n");
        return -1;
    }

    disk_t d;
    if (disk_open(&d, imgPath, "r+b", EXT2SIM_BLOCK_SIZE) != 0) {
        fprintf(stderr, "mkdir: failed to open image\n");
        return -1;
    }

    ext2sim_superblock sb;
    if (load_superblock(&d, &sb) != 0) {
        fprintf(stderr, "mkdir: failed to load superblock\n");
        disk_close(&d);
        return -1;
    }

    u32 parent = 0;
    char leaf[EXT2SIM_NAME_MAX + 1];

    if (path_parent(&d, &sb, absPath, &parent, leaf) != 0) {
        fprintf(stderr, "mkdir: invalid path or parent not found\n");
        disk_close(&d);
        return -1;
    }

    if (strcmp(leaf, ".") == 0 || strcmp(leaf, "..") == 0) {
        fprintf(stderr, "mkdir: invalid name '%s'\n", leaf);
        disk_close(&d);
        return -1;
    }

    ext2sim_inode parent_in;
    if (inode_read(&d, &sb, parent, &parent_in) != 0) {
        fprintf(stderr, "mkdir: failed to read parent inode\n");
        disk_close(&d);
        return -1;
    }
    if (!isDir(&parent_in)) {
        fprintf(stderr, "mkdir: parent is not a directory\n");
        disk_close(&d);
        return -1;
    }

    if (dirent_lookup(&d, &sb, parent, leaf) != 0) {
        fprintf(stderr, "mkdir: already exists\n");
        disk_close(&d);
        return -1;
    }

    u32 new_ino = alloc_inode(&d, &sb);
    if (new_ino == 0) {
        fprintf(stderr, "mkdir: no free inodes\n");
        disk_close(&d);
        return -1;
    }

    u32 blk = alloc_block(&d, &sb);
    if (blk == 0) {
        fprintf(stderr, "mkdir: no free blocks\n");
        free_inode(&d, &sb, (int)new_ino);
        disk_close(&d);
        return -1;
    }

    // Initialize new directory inode
    ext2sim_inode nd;
    memset(&nd, 0, sizeof(nd));
    nd.mode = (u16)(EXT2SIM_DIR | 0755);
    nd.links_count = 2;               // "." and parent's entry
    nd.size = sb.block_size;          // one block allocated
    nd.direct[0] = blk;

    if (inode_write(&d, &sb, new_ino, &nd) != 0) {
        fprintf(stderr, "mkdir: inode_write failed\n");
        free_block(&d, &sb, (int)blk);
        free_inode(&d, &sb, (int)new_ino);
        disk_close(&d);
        return -1;
    }
    jsfortest(2);
    // initialize directory block entries
    u8 buf[EXT2SIM_BLOCK_SIZE];
    memset(buf, 0, sizeof(buf));
    ext2sim_dirent* ent = (ext2sim_dirent*)buf;

    ent[0].inode = new_ino;
    memset(ent[0].name, 0, sizeof(ent[0].name));
    strncpy((char*)ent[0].name, ".", EXT2SIM_NAME_MAX);

    ent[1].inode = parent;
    memset(ent[1].name, 0, sizeof(ent[1].name));
    strncpy((char*)ent[1].name, "..", EXT2SIM_NAME_MAX);

    if (disk_write_block(&d, blk, buf) != 0) {
        fprintf(stderr, "mkdir: failed to write dir block\n");
        free_block(&d, &sb, (int)blk);
        free_inode(&d, &sb, (int)new_ino);
        disk_close(&d);
        return -1;
    }
    jsfortest(3);
    // Add entry in parent directory
    if (dirent_add(&d, &sb, parent, leaf, new_ino) != 0) {
        fprintf(stderr, "mkdir: failed to add entry to parent\n");
        free_block(&d, &sb, (int)blk);
        free_inode(&d, &sb, (int)new_ino);
        disk_close(&d);
        return -1;
    }

    // Update parent link count +1
    parent_in.links_count += 1;
    if (inode_write(&d, &sb, parent, &parent_in) != 0) {
        fprintf(stderr, "mkdir: warning: failed to update parent link count\n");
    }

    save_superblock(&d, &sb);
    disk_close(&d);
    return 0;
}