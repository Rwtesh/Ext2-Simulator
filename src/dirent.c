#include "../include/dirent.h"
#include "../include/alloc.h"
#include "../include/inode.h"

#include <stdio.h>
#include <string.h>

static int isValid(const char* name)
{
    if(!name)return 0;
    size_t l = strlen(name);
    if(l==0)return 0;
    if(l > EXT2SIM_NAME_MAX)return 0;
    if(strchr(name,'/'))return 0;
    return 1;
}

u32 dirent_lookup(disk_t* d,ext2sim_superblock* sb, u32 dir_no, const char* name)
{
    if(!d || !sb || !isValid(name))return 0;
    
    ext2sim_inode dir;
    if(inode_read(d,sb,dir_no,&dir))return 0;

    u32 blocks_used = (dir.size + sb->block_size -1) / sb->block_size;
    if(blocks_used>12)blocks_used=12;
    u8 buf[EXT2SIM_BLOCK_SIZE];
    for(u32 i=0;i<blocks_used;i++)
    {
        u32 blk = dir.direct[i];
        if(blk==0)continue;
        if(disk_read_block(d,blk,buf))return 0;
        ext2sim_dirent* ent = (ext2sim_dirent*)buf;
        for(u32 j=0;j<DIRENTS_PER_BLOCK;j++)
        {
            if(ent[j].inode==0)continue;
            ent[j].name[EXT2SIM_NAME_MAX]='\0';
            if(!strncmp(ent[j].name,name,EXT2SIM_NAME_MAX))return ent[j].inode;
        }
    }
    return 0;
}
int dirent_add(disk_t* d,ext2sim_superblock* sb, u32 dir_no,const char* name, u32 ino)
{
    if (!d || !sb || !isValid(name)) return -1;
    if (ino == 0 || ino > sb->total_inodes) return -1;
    ext2sim_inode dir;
    if (inode_read(d, sb, dir_no, &dir) != 0) return -1;

    if (dirent_lookup(d, sb, dir_no, name) != 0) {
        fprintf(stderr,"entry already exists\n");
        return -1;
    }

    u8 buf[EXT2SIM_BLOCK_SIZE];
    u32 blocks_used = (dir.size + sb->block_size - 1) / sb->block_size;
    if (blocks_used > 12) blocks_used = 12;

    for (u32 bi = 0; bi < blocks_used; bi++) {
        u32 blk = dir.direct[bi];
        if (blk == 0) continue;

        if (disk_read_block(d, blk, buf) != 0) return -1;

        ext2sim_dirent* ent = (ext2sim_dirent*)buf;
        for (u32 ei = 0; ei < DIRENTS_PER_BLOCK; ei++) {
            if (ent[ei].inode != 0) continue;
            ent[ei].inode = ino;
            memset(ent[ei].name, 0, sizeof(ent[ei].name));
            strncpy((char*)ent[ei].name, name, EXT2SIM_NAME_MAX);
            ent[ei].name[EXT2SIM_NAME_MAX] = '\0';

            if (disk_write_block(d, blk, buf) != 0) return -1;
            return 0;
        }
    }

    u32 free_direct = 12;
    for (u32 i = 0; i < 12; i++) {
        if (dir.direct[i] == 0) {
            free_direct = i;
            break;
        }
    }
    if (free_direct == 12) {
        fprintf(stderr, "directory has no free direct slots\n");
        return -1;
    }

    u32 newblk = alloc_block(d, sb);
    if (newblk == 0) return -1;

    dir.direct[free_direct] = newblk;
    dir.size += sb->block_size;

    if (inode_write(d, sb, dir_no, &dir) != 0) return -1;
    if (disk_read_block(d, newblk, buf) != 0) return -1;

    ext2sim_dirent* ent = (ext2sim_dirent*)buf;
    ent[0].inode = ino;
    memset(ent[0].name, 0, sizeof(ent[0].name));
    strncpy((char*)ent[0].name, name, EXT2SIM_NAME_MAX);
    ent[0].name[EXT2SIM_NAME_MAX] = '\0';

    if (disk_write_block(d, newblk, buf) != 0) return -1;

    return 0;
}