/*
 * fuse_fs.c – FUSE3 mountable interface for the ext2sim filesystem.
 *
 * Build:  see Makefile (target ext2sim-fuse)
 * Usage:  ext2sim-fuse <image> <mountpoint> [FUSE options]
 *         ext2sim-fuse disk.img /mnt/ext2 -f          # foreground
 *         ext2sim-fuse disk.img /mnt/ext2 -o ro       # read-only
 * Unmount: fusermount3 -u <mountpoint>
 */

#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>

#include "../include/alloc.h"
#include "../include/dirent.h"
#include "../include/disk.h"
#include "../include/ext2sim.h"
#include "../include/inode.h"
#include "../include/path.h"

/* ------------------------------------------------------------------ */
/*  Mount context                                                       */
/* ------------------------------------------------------------------ */

typedef struct {
    disk_t             disk;
    ext2sim_superblock sb;
    int                read_only;  /* non-zero when opened read-only  */
    pthread_mutex_t    lock;       /* serialises all disk operations   */
} fuse_ctx_t;

static fuse_ctx_t *get_ctx(void)
{
    return (fuse_ctx_t *)fuse_get_context()->private_data;
}

/* ------------------------------------------------------------------ */
/*  getattr (stat)                                                      */
/* ------------------------------------------------------------------ */

static int ext2sim_getattr(const char *path, struct stat *st,
                           struct fuse_file_info *fi)
{
    (void)fi;
    fuse_ctx_t *ctx = get_ctx();
    memset(st, 0, sizeof(*st));

    pthread_mutex_lock(&ctx->lock);

    u32 ino = path_resolve(&ctx->disk, &ctx->sb, path);
    if (ino == 0) {
        pthread_mutex_unlock(&ctx->lock);
        return -ENOENT;
    }

    ext2sim_inode in;
    if (inode_read(&ctx->disk, &ctx->sb, ino, &in) != 0) {
        pthread_mutex_unlock(&ctx->lock);
        return -EIO;
    }

    pthread_mutex_unlock(&ctx->lock);

    if ((in.mode & EXT2SIM_MT) == EXT2SIM_DIR) {
        st->st_mode  = S_IFDIR | 0755;
        st->st_nlink = (nlink_t)(in.links_count ? in.links_count : 2);
    } else {
        st->st_mode  = S_IFREG | 0644;
        st->st_nlink = (nlink_t)(in.links_count ? in.links_count : 1);
    }

    st->st_size    = (off_t)in.size;
    st->st_ino     = (ino_t)ino;
    st->st_blksize = (blksize_t)EXT2SIM_BLOCK_SIZE;
    st->st_blocks  = (blkcnt_t)(((uint64_t)in.size + 511u) / 512u);

    return 0;
}

/* ------------------------------------------------------------------ */
/*  readdir (ls)                                                        */
/* ------------------------------------------------------------------ */

static int ext2sim_readdir(const char *path, void *buf,
                           fuse_fill_dir_t filler, off_t offset,
                           struct fuse_file_info *fi,
                           enum fuse_readdir_flags flags)
{
    (void)offset;
    (void)fi;
    (void)flags;

    fuse_ctx_t *ctx = get_ctx();

    pthread_mutex_lock(&ctx->lock);

    u32 ino = path_resolve(&ctx->disk, &ctx->sb, path);
    if (ino == 0) {
        pthread_mutex_unlock(&ctx->lock);
        return -ENOENT;
    }

    ext2sim_inode dir;
    if (inode_read(&ctx->disk, &ctx->sb, ino, &dir) != 0) {
        pthread_mutex_unlock(&ctx->lock);
        return -EIO;
    }

    if ((dir.mode & EXT2SIM_MT) != EXT2SIM_DIR) {
        pthread_mutex_unlock(&ctx->lock);
        return -ENOTDIR;
    }

    /* Always emit "." and ".." so the kernel is satisfied. */
    filler(buf, ".",  NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    u32 block_size  = ctx->sb.block_size;
    u32 blocks_used = (dir.size + block_size - 1u) / block_size;
    if (blocks_used > 12u) blocks_used = 12u;

    u8 blkbuf[EXT2SIM_BLOCK_SIZE];

    for (u32 bi = 0; bi < blocks_used; bi++) {
        u32 blk = dir.direct[bi];
        if (blk == 0) continue;

        if (disk_read_block(&ctx->disk, blk, blkbuf) != 0) {
            pthread_mutex_unlock(&ctx->lock);
            return -EIO;
        }

        ext2sim_dirent *ents = (ext2sim_dirent *)blkbuf;
        for (u32 ei = 0; ei < DIRENTS_PER_BLOCK; ei++) {
            if (ents[ei].inode == 0) continue;
            ents[ei].name[EXT2SIM_NAME_MAX] = '\0';
            /* Skip . and ..: already emitted above. */
            if (strcmp(ents[ei].name, ".")  == 0 ||
                strcmp(ents[ei].name, "..") == 0)
                continue;
            filler(buf, ents[ei].name, NULL, 0, 0);
        }
    }

    pthread_mutex_unlock(&ctx->lock);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  open                                                                */
/* ------------------------------------------------------------------ */

static int ext2sim_open(const char *path, struct fuse_file_info *fi)
{
    fuse_ctx_t *ctx = get_ctx();

    pthread_mutex_lock(&ctx->lock);
    u32 ino = path_resolve(&ctx->disk, &ctx->sb, path);
    if (ino == 0) {
        pthread_mutex_unlock(&ctx->lock);
        return -ENOENT;
    }

    ext2sim_inode in;
    if (inode_read(&ctx->disk, &ctx->sb, ino, &in) != 0) {
        pthread_mutex_unlock(&ctx->lock);
        return -EIO;
    }
    pthread_mutex_unlock(&ctx->lock);

    if ((in.mode & EXT2SIM_MT) == EXT2SIM_DIR) return -EISDIR;

    if (ctx->read_only && (fi->flags & O_ACCMODE) != O_RDONLY)
        return -EROFS;

    return 0;
}

/* ------------------------------------------------------------------ */
/*  read                                                                */
/* ------------------------------------------------------------------ */

static int ext2sim_read(const char *path, char *buf, size_t size,
                        off_t offset, struct fuse_file_info *fi)
{
    (void)fi;
    if (offset < 0) return -EINVAL;

    fuse_ctx_t *ctx = get_ctx();

    pthread_mutex_lock(&ctx->lock);

    u32 ino = path_resolve(&ctx->disk, &ctx->sb, path);
    if (ino == 0) {
        pthread_mutex_unlock(&ctx->lock);
        return -ENOENT;
    }

    ext2sim_inode in;
    if (inode_read(&ctx->disk, &ctx->sb, ino, &in) != 0) {
        pthread_mutex_unlock(&ctx->lock);
        return -EIO;
    }

    if ((in.mode & EXT2SIM_MT) != EXT2SIM_REG) {
        pthread_mutex_unlock(&ctx->lock);
        return -EISDIR;
    }

    u32 file_size  = in.size;
    u32 block_size = ctx->sb.block_size;

    if ((uint64_t)offset >= (uint64_t)file_size) {
        pthread_mutex_unlock(&ctx->lock);
        return 0;
    }

    u32 uoffset   = (u32)offset;
    u32 remaining = file_size - uoffset;
    if (size > (size_t)remaining) size = (size_t)remaining;

    u8     blkbuf[EXT2SIM_BLOCK_SIZE];
    size_t total_read = 0;

    while (total_read < size) {
        u32 cur = uoffset + (u32)total_read;
        u32 bi  = cur / block_size;
        if (bi >= 12u) break;

        u32 blk = in.direct[bi];
        if (blk == 0) break;

        if (disk_read_block(&ctx->disk, blk, blkbuf) != 0) {
            pthread_mutex_unlock(&ctx->lock);
            return -EIO;
        }

        u32 off_in_block = cur % block_size;
        u32 avail        = block_size - off_in_block;
        u32 to_copy      = (u32)(size - total_read);
        if (to_copy > avail) to_copy = avail;

        memcpy(buf + total_read, blkbuf + off_in_block, to_copy);
        total_read += to_copy;
    }

    pthread_mutex_unlock(&ctx->lock);
    return (int)total_read;
}

/* ------------------------------------------------------------------ */
/*  write                                                               */
/* ------------------------------------------------------------------ */

static int ext2sim_write(const char *path, const char *buf, size_t size,
                         off_t offset, struct fuse_file_info *fi)
{
    (void)fi;
    if (offset < 0) return -EINVAL;

    fuse_ctx_t *ctx = get_ctx();
    if (ctx->read_only) return -EROFS;

    pthread_mutex_lock(&ctx->lock);

    u32 ino = path_resolve(&ctx->disk, &ctx->sb, path);
    if (ino == 0) {
        pthread_mutex_unlock(&ctx->lock);
        return -ENOENT;
    }

    ext2sim_inode in;
    if (inode_read(&ctx->disk, &ctx->sb, ino, &in) != 0) {
        pthread_mutex_unlock(&ctx->lock);
        return -EIO;
    }

    if ((in.mode & EXT2SIM_MT) != EXT2SIM_REG) {
        pthread_mutex_unlock(&ctx->lock);
        return -EISDIR;
    }

    u32 block_size = ctx->sb.block_size;
    u32 max_size   = 12u * block_size;

    if ((uint64_t)offset >= (uint64_t)max_size) {
        pthread_mutex_unlock(&ctx->lock);
        return -EFBIG;
    }

    u32 uoffset = (u32)offset;
    u32 space   = max_size - uoffset;
    if (size > (size_t)space) size = (size_t)space;

    u8     blkbuf[EXT2SIM_BLOCK_SIZE];
    size_t total_written = 0;

    while (total_written < size) {
        u32 cur = uoffset + (u32)total_written;
        u32 bi  = cur / block_size;
        if (bi >= 12u) break;

        /* Allocate block on demand. */
        if (in.direct[bi] == 0) {
            u32 newblk = alloc_block(&ctx->disk, &ctx->sb);
            if (newblk == 0) break; /* out of space */
            disk_zero_block(&ctx->disk, newblk);
            in.direct[bi] = newblk;
        }

        u32 blk          = in.direct[bi];
        u32 off_in_block = cur % block_size;
        u32 avail        = block_size - off_in_block;
        u32 to_copy      = (u32)(size - total_written);
        if (to_copy > avail) to_copy = avail;

        /* Read-modify-write for partial block writes. */
        if (disk_read_block(&ctx->disk, blk, blkbuf) != 0) {
            pthread_mutex_unlock(&ctx->lock);
            return -EIO;
        }
        memcpy(blkbuf + off_in_block, buf + total_written, to_copy);
        if (disk_write_block(&ctx->disk, blk, blkbuf) != 0) {
            pthread_mutex_unlock(&ctx->lock);
            return -EIO;
        }

        total_written += to_copy;
    }

    /* Extend file size if we wrote past EOF. */
    u32 new_end = uoffset + (u32)total_written;
    if (new_end > in.size) in.size = new_end;

    if (inode_write(&ctx->disk, &ctx->sb, ino, &in) != 0) {
        pthread_mutex_unlock(&ctx->lock);
        return -EIO;
    }
    save_superblock(&ctx->disk, &ctx->sb);

    pthread_mutex_unlock(&ctx->lock);
    return (int)total_written;
}

/* ------------------------------------------------------------------ */
/*  truncate                                                            */
/* ------------------------------------------------------------------ */

static int ext2sim_truncate(const char *path, off_t size,
                            struct fuse_file_info *fi)
{
    (void)fi;
    if (size < 0) return -EINVAL;

    fuse_ctx_t *ctx = get_ctx();
    if (ctx->read_only) return -EROFS;

    pthread_mutex_lock(&ctx->lock);

    u32 ino = path_resolve(&ctx->disk, &ctx->sb, path);
    if (ino == 0) {
        pthread_mutex_unlock(&ctx->lock);
        return -ENOENT;
    }

    ext2sim_inode in;
    if (inode_read(&ctx->disk, &ctx->sb, ino, &in) != 0) {
        pthread_mutex_unlock(&ctx->lock);
        return -EIO;
    }

    if ((in.mode & EXT2SIM_MT) != EXT2SIM_REG) {
        pthread_mutex_unlock(&ctx->lock);
        return -EINVAL;
    }

    u32      block_size = ctx->sb.block_size;
    u32      max_size   = 12u * block_size;
    uint64_t new_sz64   = (uint64_t)size;

    if (new_sz64 > (uint64_t)max_size) {
        pthread_mutex_unlock(&ctx->lock);
        return -EFBIG;
    }

    u32 new_sz  = (u32)new_sz64;
    u32 needed  = new_sz ? (new_sz + block_size - 1u) / block_size : 0u;

    /* Free blocks beyond the new size. */
    for (u32 i = needed; i < 12u; i++) {
        if (in.direct[i] != 0) {
            free_block(&ctx->disk, &ctx->sb, in.direct[i]);
            in.direct[i] = 0;
        }
    }

    in.size = new_sz;
    if (inode_write(&ctx->disk, &ctx->sb, ino, &in) != 0) {
        pthread_mutex_unlock(&ctx->lock);
        return -EIO;
    }
    save_superblock(&ctx->disk, &ctx->sb);

    pthread_mutex_unlock(&ctx->lock);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  create                                                              */
/* ------------------------------------------------------------------ */

static int ext2sim_create(const char *path, mode_t mode,
                          struct fuse_file_info *fi)
{
    (void)mode;
    (void)fi;

    fuse_ctx_t *ctx = get_ctx();
    if (ctx->read_only) return -EROFS;

    pthread_mutex_lock(&ctx->lock);

    u32  parent_ino = 0;
    char leaf[EXT2SIM_NAME_MAX + 1];

    if (path_parent(&ctx->disk, &ctx->sb, path, &parent_ino, leaf) != 0) {
        pthread_mutex_unlock(&ctx->lock);
        return -ENOENT;
    }

    if (dirent_lookup(&ctx->disk, &ctx->sb, parent_ino, leaf) != 0) {
        pthread_mutex_unlock(&ctx->lock);
        return -EEXIST;
    }

    u32 ino = alloc_inode(&ctx->disk, &ctx->sb);
    if (ino == 0) {
        pthread_mutex_unlock(&ctx->lock);
        return -ENOSPC;
    }

    ext2sim_inode f;
    memset(&f, 0, sizeof(f));
    f.mode        = (u16)(EXT2SIM_REG | 0644);
    f.links_count = 1;
    f.size        = 0;

    if (inode_write(&ctx->disk, &ctx->sb, ino, &f) != 0) {
        free_inode(&ctx->disk, &ctx->sb, ino);
        pthread_mutex_unlock(&ctx->lock);
        return -EIO;
    }

    if (dirent_add(&ctx->disk, &ctx->sb, parent_ino, leaf, ino) != 0) {
        free_inode(&ctx->disk, &ctx->sb, ino);
        pthread_mutex_unlock(&ctx->lock);
        return -EIO;
    }

    save_superblock(&ctx->disk, &ctx->sb);
    pthread_mutex_unlock(&ctx->lock);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  unlink                                                              */
/* ------------------------------------------------------------------ */

static int ext2sim_unlink(const char *path)
{
    fuse_ctx_t *ctx = get_ctx();
    if (ctx->read_only) return -EROFS;

    pthread_mutex_lock(&ctx->lock);

    u32  parent_ino = 0;
    char leaf[EXT2SIM_NAME_MAX + 1];

    if (path_parent(&ctx->disk, &ctx->sb, path, &parent_ino, leaf) != 0) {
        pthread_mutex_unlock(&ctx->lock);
        return -ENOENT;
    }

    u32 ino = dirent_lookup(&ctx->disk, &ctx->sb, parent_ino, leaf);
    if (ino == 0) {
        pthread_mutex_unlock(&ctx->lock);
        return -ENOENT;
    }

    ext2sim_inode in;
    if (inode_read(&ctx->disk, &ctx->sb, ino, &in) != 0) {
        pthread_mutex_unlock(&ctx->lock);
        return -EIO;
    }

    if ((in.mode & EXT2SIM_MT) == EXT2SIM_DIR) {
        pthread_mutex_unlock(&ctx->lock);
        return -EISDIR;
    }

    if (dirent_remove(&ctx->disk, &ctx->sb, parent_ino, leaf) != 0) {
        pthread_mutex_unlock(&ctx->lock);
        return -EIO;
    }

    for (u32 i = 0; i < 12u; i++) {
        if (in.direct[i] != 0)
            free_block(&ctx->disk, &ctx->sb, in.direct[i]);
    }

    free_inode(&ctx->disk, &ctx->sb, ino);
    save_superblock(&ctx->disk, &ctx->sb);

    pthread_mutex_unlock(&ctx->lock);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  mkdir                                                               */
/* ------------------------------------------------------------------ */

static int ext2sim_mkdir(const char *path, mode_t mode)
{
    (void)mode;

    fuse_ctx_t *ctx = get_ctx();
    if (ctx->read_only) return -EROFS;

    pthread_mutex_lock(&ctx->lock);

    u32  parent_ino = 0;
    char leaf[EXT2SIM_NAME_MAX + 1];

    if (path_parent(&ctx->disk, &ctx->sb, path, &parent_ino, leaf) != 0) {
        pthread_mutex_unlock(&ctx->lock);
        return -ENOENT;
    }

    if (dirent_lookup(&ctx->disk, &ctx->sb, parent_ino, leaf) != 0) {
        pthread_mutex_unlock(&ctx->lock);
        return -EEXIST;
    }

    u32 new_ino = alloc_inode(&ctx->disk, &ctx->sb);
    if (new_ino == 0) {
        pthread_mutex_unlock(&ctx->lock);
        return -ENOSPC;
    }

    u32 blk = alloc_block(&ctx->disk, &ctx->sb);
    if (blk == 0) {
        free_inode(&ctx->disk, &ctx->sb, new_ino);
        pthread_mutex_unlock(&ctx->lock);
        return -ENOSPC;
    }

    ext2sim_inode nd;
    memset(&nd, 0, sizeof(nd));
    nd.mode        = (u16)(EXT2SIM_DIR | 0755);
    nd.links_count = 2;
    nd.size        = ctx->sb.block_size;
    nd.direct[0]   = blk;

    if (inode_write(&ctx->disk, &ctx->sb, new_ino, &nd) != 0) {
        free_block(&ctx->disk, &ctx->sb, blk);
        free_inode(&ctx->disk, &ctx->sb, new_ino);
        pthread_mutex_unlock(&ctx->lock);
        return -EIO;
    }

    /* Initialise the directory block with "." and ".." entries. */
    u8 dbuf[EXT2SIM_BLOCK_SIZE];
    memset(dbuf, 0, sizeof(dbuf));
    ext2sim_dirent *ent = (ext2sim_dirent *)dbuf;

    ent[0].inode = new_ino;
    strncpy(ent[0].name, ".",  EXT2SIM_NAME_MAX);
    ent[0].name[EXT2SIM_NAME_MAX] = '\0';

    ent[1].inode = parent_ino;
    strncpy(ent[1].name, "..", EXT2SIM_NAME_MAX);
    ent[1].name[EXT2SIM_NAME_MAX] = '\0';

    if (disk_write_block(&ctx->disk, blk, dbuf) != 0) {
        free_block(&ctx->disk, &ctx->sb, blk);
        free_inode(&ctx->disk, &ctx->sb, new_ino);
        pthread_mutex_unlock(&ctx->lock);
        return -EIO;
    }

    if (dirent_add(&ctx->disk, &ctx->sb, parent_ino, leaf, new_ino) != 0) {
        free_block(&ctx->disk, &ctx->sb, blk);
        free_inode(&ctx->disk, &ctx->sb, new_ino);
        pthread_mutex_unlock(&ctx->lock);
        return -EIO;
    }

    /* Bump parent link count (one extra ".." points at it). */
    ext2sim_inode parent_in;
    if (inode_read(&ctx->disk, &ctx->sb, parent_ino, &parent_in) == 0) {
        parent_in.links_count++;
        inode_write(&ctx->disk, &ctx->sb, parent_ino, &parent_in);
    }

    save_superblock(&ctx->disk, &ctx->sb);
    pthread_mutex_unlock(&ctx->lock);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  rmdir                                                               */
/* ------------------------------------------------------------------ */

static int ext2sim_rmdir(const char *path)
{
    fuse_ctx_t *ctx = get_ctx();
    if (ctx->read_only) return -EROFS;

    pthread_mutex_lock(&ctx->lock);

    u32  parent_ino = 0;
    char leaf[EXT2SIM_NAME_MAX + 1];

    if (path_parent(&ctx->disk, &ctx->sb, path, &parent_ino, leaf) != 0) {
        pthread_mutex_unlock(&ctx->lock);
        return -ENOENT;
    }

    u32 ino = dirent_lookup(&ctx->disk, &ctx->sb, parent_ino, leaf);
    if (ino == 0) {
        pthread_mutex_unlock(&ctx->lock);
        return -ENOENT;
    }

    ext2sim_inode in;
    if (inode_read(&ctx->disk, &ctx->sb, ino, &in) != 0) {
        pthread_mutex_unlock(&ctx->lock);
        return -EIO;
    }

    if ((in.mode & EXT2SIM_MT) != EXT2SIM_DIR) {
        pthread_mutex_unlock(&ctx->lock);
        return -ENOTDIR;
    }

    /* Verify the directory is empty (only "." and ".." allowed). */
    u32 block_size  = ctx->sb.block_size;
    u32 blocks_used = (in.size + block_size - 1u) / block_size;
    if (blocks_used > 12u) blocks_used = 12u;

    u8 blkbuf[EXT2SIM_BLOCK_SIZE];
    for (u32 bi = 0; bi < blocks_used; bi++) {
        u32 blk = in.direct[bi];
        if (blk == 0) continue;
        if (disk_read_block(&ctx->disk, blk, blkbuf) != 0) {
            pthread_mutex_unlock(&ctx->lock);
            return -EIO;
        }
        ext2sim_dirent *ents = (ext2sim_dirent *)blkbuf;
        for (u32 ei = 0; ei < DIRENTS_PER_BLOCK; ei++) {
            if (ents[ei].inode == 0) continue;
            ents[ei].name[EXT2SIM_NAME_MAX] = '\0';
            if (strcmp(ents[ei].name, ".")  == 0 ||
                strcmp(ents[ei].name, "..") == 0)
                continue;
            pthread_mutex_unlock(&ctx->lock);
            return -ENOTEMPTY;
        }
    }

    if (dirent_remove(&ctx->disk, &ctx->sb, parent_ino, leaf) != 0) {
        pthread_mutex_unlock(&ctx->lock);
        return -EIO;
    }

    for (u32 i = 0; i < 12u; i++) {
        if (in.direct[i] != 0)
            free_block(&ctx->disk, &ctx->sb, in.direct[i]);
    }

    free_inode(&ctx->disk, &ctx->sb, ino);

    /* Decrease parent link count (one fewer ".." pointing at it). */
    ext2sim_inode parent_in;
    if (inode_read(&ctx->disk, &ctx->sb, parent_ino, &parent_in) == 0) {
        if (parent_in.links_count > 0) parent_in.links_count--;
        inode_write(&ctx->disk, &ctx->sb, parent_ino, &parent_in);
    }

    save_superblock(&ctx->disk, &ctx->sb);
    pthread_mutex_unlock(&ctx->lock);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  statfs (df)                                                         */
/* ------------------------------------------------------------------ */

static int ext2sim_statfs(const char *path, struct statvfs *stv)
{
    (void)path;
    fuse_ctx_t        *ctx = get_ctx();
    ext2sim_superblock *sb  = &ctx->sb;

    pthread_mutex_lock(&ctx->lock);

    memset(stv, 0, sizeof(*stv));
    stv->f_bsize   = sb->block_size;
    stv->f_frsize  = sb->block_size;
    stv->f_blocks  = (fsblkcnt_t)sb->total_blocks;
    stv->f_bfree   = (fsblkcnt_t)sb->free_blocks;
    stv->f_bavail  = (fsblkcnt_t)sb->free_blocks;
    stv->f_files   = (fsfilcnt_t)sb->total_inodes;
    stv->f_ffree   = (fsfilcnt_t)sb->free_inodes;
    stv->f_namemax = EXT2SIM_NAME_MAX;

    pthread_mutex_unlock(&ctx->lock);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  FUSE operations table                                               */
/* ------------------------------------------------------------------ */

static const struct fuse_operations ext2sim_ops = {
    .getattr  = ext2sim_getattr,
    .readdir  = ext2sim_readdir,
    .open     = ext2sim_open,
    .read     = ext2sim_read,
    .write    = ext2sim_write,
    .truncate = ext2sim_truncate,
    .create   = ext2sim_create,
    .unlink   = ext2sim_unlink,
    .mkdir    = ext2sim_mkdir,
    .rmdir    = ext2sim_rmdir,
    .statfs   = ext2sim_statfs,
};

/* ------------------------------------------------------------------ */
/*  main                                                                */
/* ------------------------------------------------------------------ */

static void usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s <image> <mountpoint> [FUSE options]\n\n"
        "  <image>       Path to the ext2sim disk image\n"
        "  <mountpoint>  Directory to mount the filesystem at\n\n"
        "Common FUSE options:\n"
        "  -f            Run in the foreground (do not daemonise)\n"
        "  -d            Debug mode (implies -f)\n"
        "  -o ro         Mount read-only\n"
        "  -s            Single-threaded mode\n\n"
        "Unmount: fusermount3 -u <mountpoint>\n",
        prog);
}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        usage(argv[0]);
        return 1;
    }

    char *image_path = argv[1];

    /*
     * Build a new argv for fuse_main that omits the image path
     * (argv[1]), so FUSE sees: { prog, mountpoint, [options...] }.
     */
    int    fuse_argc = argc - 1;
    char **fuse_argv = (char **)malloc(((size_t)fuse_argc + 1u) * sizeof(char *));
    if (!fuse_argv) {
        fprintf(stderr, "ext2sim-fuse: out of memory\n");
        return 1;
    }
    fuse_argv[0] = argv[0];
    for (int i = 2; i < argc; i++)
        fuse_argv[i - 1] = argv[i];
    fuse_argv[fuse_argc] = NULL;

    /*
     * Initialise mount context and open the disk image.
     * Default to read-write; pass "-o ro" to FUSE for a read-only mount.
     */
    fuse_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.read_only = 0;
    pthread_mutex_init(&ctx.lock, NULL);

    if (disk_open(&ctx.disk, image_path, "r+b", EXT2SIM_BLOCK_SIZE) != 0) {
        fprintf(stderr, "ext2sim-fuse: cannot open image '%s'\n", image_path);
        free(fuse_argv);
        return 1;
    }

    if (load_superblock(&ctx.disk, &ctx.sb) != 0) {
        fprintf(stderr,
                "ext2sim-fuse: '%s' does not look like an ext2sim image "
                "(bad magic)\n", image_path);
        disk_close(&ctx.disk);
        free(fuse_argv);
        return 1;
    }

    int ret = fuse_main(fuse_argc, fuse_argv, &ext2sim_ops, &ctx);

    disk_close(&ctx.disk);
    pthread_mutex_destroy(&ctx.lock);
    free(fuse_argv);
    return ret;
}
