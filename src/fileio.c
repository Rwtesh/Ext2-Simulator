#include "../include/fileio.h"
#include "../include/disk.h"
#include "../include/alloc.h"
#include "../include/inode.h"
#include "../include/path.h"
#include "../include/ext2sim.h"

#include <stdio.h>
#include <string.h>

int isReg(ext2sim_inode* in)
{
    return ((in->mode & EXT2SIM_MT) == EXT2SIM_REG);
}

int cmd_write(const char* imgPath, const char* absPath, const void* data, u32 len)
{
    if (!imgPath || !absPath) return -1;
    if (absPath[0] != '/') {
        fprintf(stderr, "write: only absolute paths supported\n");
        return -1;
    }

    disk_t d;
    if (disk_open(&d, imgPath, "r+b", EXT2SIM_BLOCK_SIZE) != 0) {
        fprintf(stderr, "write: failed to open image\n");
        return -1;
    }

    ext2sim_superblock sb;
    if (load_superblock(&d, &sb) != 0) {
        fprintf(stderr, "write: failed to load superblock\n");
        disk_close(&d);
        return -1;
    }

    u32 ino = path_resolve(&d, &sb, absPath);
    if (ino == 0) {
        fprintf(stderr, "write: file not found: %s\n", absPath);
        disk_close(&d);
        return -1;
    }

    ext2sim_inode in;
    if (inode_read(&d, &sb, ino, &in) != 0) {
        fprintf(stderr, "write: inode_read failed\n");
        disk_close(&d);
        return -1;
    }

    if (!isReg(&in)) {
        fprintf(stderr, "write: not a regular file\n");
        disk_close(&d);
        return -1;
    }

    u32 max_bytes = 12 * sb.block_size;
    if (len > max_bytes) {
        fprintf(stderr, "write: too large (max %u bytes)\n", max_bytes);
        disk_close(&d);
        return -1;
    }

    // Free old blocks (overwrite semantics)
    for (u32 i = 0; i < 12; i++) {
        if (in.direct[i] != 0) {
            free_block(&d, &sb, (int)in.direct[i]);
            in.direct[i] = 0;
        }
    }

    // Allocate + write new blocks
    u32 needed = (len + sb.block_size - 1) / sb.block_size;
    const u8* p = (const u8*)data;

    u8 buf[EXT2SIM_BLOCK_SIZE];

    for (u32 bi = 0; bi < needed; bi++) {
        u32 blk = alloc_block(&d, &sb);
        if (blk == 0) {
            fprintf(stderr, "write: out of blocks\n");
            // Leak note: we already freed old blocks; also free what we allocated so far
            for (u32 k = 0; k < bi; k++) {
                if (in.direct[k] != 0) free_block(&d, &sb, (int)in.direct[k]);
                in.direct[k] = 0;
            }
            disk_close(&d);
            return -1;
        }

        in.direct[bi] = blk;

        u32 off = bi * sb.block_size;
        u32 remain = (len > off) ? (len - off) : 0;
        u32 chunk = (remain > sb.block_size) ? sb.block_size : remain;

        memset(buf, 0, sb.block_size);
        if (chunk > 0) memcpy(buf, p + off, chunk);

        if (disk_write_block(&d, blk, buf) != 0) {
            fprintf(stderr, "write: disk_write_block failed\n");
            disk_close(&d);
            return -1;
        }
    }

    in.size = len;

    if (inode_write(&d, &sb, ino, &in) != 0) {
        fprintf(stderr, "write: inode_write failed\n");
        disk_close(&d);
        return -1;
    }

    save_superblock(&d, &sb);
    disk_close(&d);
    return 0;
}

int cmd_cat(const char* imgPath, const char* absPath)
{
    if (!imgPath || !absPath) return -1;
    if (absPath[0] != '/') {
        fprintf(stderr, "cat: only absolute paths supported\n");
        return -1;
    }

    disk_t d;
    if (disk_open(&d, imgPath, "rb", EXT2SIM_BLOCK_SIZE) != 0) {
        fprintf(stderr, "cat: failed to open image\n");
        return -1;
    }

    ext2sim_superblock sb;
    if (load_superblock(&d, &sb) != 0) {
        fprintf(stderr, "cat: failed to load superblock\n");
        disk_close(&d);
        return -1;
    }

    u32 ino = path_resolve(&d, &sb, absPath);
    if (ino == 0) {
        fprintf(stderr, "cat: file not found: %s\n", absPath);
        disk_close(&d);
        return -1;
    }

    ext2sim_inode in;
    if (inode_read(&d, &sb, ino, &in) != 0) {
        fprintf(stderr, "cat: inode_read failed\n");
        disk_close(&d);
        return -1;
    }

    if (!isReg(&in)) {
        fprintf(stderr, "cat: not a regular file\n");
        disk_close(&d);
        return -1;
    }

    u32 remaining = in.size;
    u8 buf[EXT2SIM_BLOCK_SIZE];

    for (u32 bi = 0; bi < 12 && remaining > 0; bi++) {
        u32 blk = in.direct[bi];
        if (blk == 0) break;

        if (disk_read_block(&d, blk, buf) != 0) {
            fprintf(stderr, "cat: disk_read_block failed\n");
            disk_close(&d);
            return -1;
        }

        u32 chunk = (remaining > sb.block_size) ? sb.block_size : remaining;
        fwrite(buf, 1, chunk, stdout);
        remaining -= chunk;
    }

    // Optional: newline for nicer display
    if (in.size > 0) fputc('\n', stdout);

    disk_close(&d);
    return 0;
}