#include "../include/path.h"
#include "../include/dirent.h"
#include "../include/inode.h"

#include <string.h>

static int check(int x)
{
    printf("hello\n");
    return x;
}
static int is_abs_path(const char* p)
{
    return p && p[0] == '/';
}
static size_t next_component(const char* path, size_t* idx, char out[EXT2SIM_NAME_MAX + 1])
{
    size_t i = *idx;
    while (path[i] == '/') i++;
    if (path[i] == '\0') {
        out[0] = '\0';
        *idx = i;
        return 0;
    }

    size_t start = i;
    while (path[i] != '\0' && path[i] != '/') i++;
    size_t len = i - start;
    if (len == 0 || len > EXT2SIM_NAME_MAX) return (size_t)-1;
    memcpy(out, path + start, len);
    out[len] = '\0';

    *idx = i;
    return len;
}

static int is_dir_inode(disk_t* d, ext2sim_superblock* sb, u32 ino)
{
    ext2sim_inode in;
    if (inode_read(d, sb, ino, &in) != 0) return 0;

    return (in.mode & EXT2SIM_S_IFDIR) == EXT2SIM_S_IFDIR;

}

static u32 step_component(disk_t* d, ext2sim_superblock* sb, u32 cur, const char* comp)
{
    if (strcmp(comp, ".") == 0) {
        return cur;
    }
    if (strcmp(comp, "..") == 0) {
        // If we're at root, keep it at root (robust behavior)
        if (cur == EXT2SIM_ROOT_INODE) return cur;

        // Resolve via the ".." directory entry
        u32 up = dirent_lookup(d, sb, cur, "..");
        if (up == 0) return 0;
        return up;
    }

    return dirent_lookup(d, sb, cur, comp);
}

u32 path_resolve(disk_t* d, ext2sim_superblock* sb, const char* abs_path)
{
    if (!d || !sb || !is_abs_path(abs_path)) return 0;

    if (strcmp(abs_path, "/") == 0) return EXT2SIM_ROOT_INODE;

    u32 cur = EXT2SIM_ROOT_INODE;
    size_t idx = 0;
    char comp[EXT2SIM_NAME_MAX + 1];

    while (1) {
        size_t len = next_component(abs_path, &idx, comp);
        if (len == 0) break;
        if (len == (size_t)-1) return 0;

        if (!is_dir_inode(d, sb, cur)) return 0;

        u32 next = step_component(d, sb, cur, comp);
        if (next == 0) return 0;

        cur = next;
    }

    return cur;
}

int path_parent(disk_t* d,ext2sim_superblock* sb,const char* abs_path,u32* parent_ino_out,char leaf_out[EXT2SIM_NAME_MAX + 1])
{
    if (!d || !sb || !parent_ino_out || !leaf_out || !is_abs_path(abs_path)) return -1;

    if (strcmp(abs_path, "/") == 0) return -1;

    u32 cur = EXT2SIM_ROOT_INODE;
    size_t idx = 0;

    char comp[EXT2SIM_NAME_MAX + 1];
    char nextcomp[EXT2SIM_NAME_MAX + 1];

    size_t len = next_component(abs_path, &idx, comp);
    if (len == 0 || len == (size_t)-1) return -1;

    while (1) {
        size_t save = idx;
        size_t nlen = next_component(abs_path, &save, nextcomp);
        if (nlen == (size_t)-1) return -1;

        if (nlen == 0) {
            // comp is leaf (we intentionally do NOT interpret "." / ".." as leaf here)
            strncpy(leaf_out, comp, EXT2SIM_NAME_MAX);
            leaf_out[EXT2SIM_NAME_MAX] = '\0';
            *parent_ino_out = cur;
            return 0;
        }

        if (!is_dir_inode(d, sb, cur)) return -1;

        u32 next = step_component(d, sb, cur, comp);
        if (next == 0) return -1;

        cur = next;

        strncpy(comp, nextcomp, EXT2SIM_NAME_MAX);
        comp[EXT2SIM_NAME_MAX] = '\0';
        idx = save;
    }
}