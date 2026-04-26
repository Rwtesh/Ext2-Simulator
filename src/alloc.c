#include "../include/alloc.h"
#include <string.h>
#include <stdio.h>

static int bitmap_get(u8* bmp,u32 idx)
{
    return (bmp[idx/8] >> (idx%8)) & 1u;
}
static void bitmap_set(u8* bmp,u32 idx)
{
    bmp[idx/8] |= (u8)(1u << (idx%8));
}
static void bitmap_clear(u8* bmp, u32 idx)
{
    bmp[idx/8] &= (u8)~(1u << (idx%8));
}

int load_superblock(disk_t* d, ext2sim_superblock* sb)
{
    if(!d || !sb) return -1;
    u8 buf[EXT2SIM_BLOCK_SIZE];
    if(disk_read_block(d,1,buf))return -1;
    memcpy(sb,buf,sizeof(*sb));
    if(sb->magic!=EXT2SIM_MAGIC)
    {
        fprintf(stderr,"Couldn't validate magic no.\n");
        return -1;
    }
    return 0;
}

int save_superblock(disk_t* d,const ext2sim_superblock* sb)
{
    if(!d || !sb)return -1;
    u8 buf[EXT2SIM_BLOCK_SIZE];
    memset(buf,0,EXT2SIM_BLOCK_SIZE);
    memcpy(buf,sb,sizeof(*sb));
    return disk_write_block(d,1,buf);
}

u32 alloc_block(disk_t* d,ext2sim_superblock* sb)
{
    if(!d || !sb)return 0;
    if(sb->free_blocks == 0)
    {
        fprintf(stderr,"No free blocks.\n");
        return 0;
    }
    u8 bbm[EXT2SIM_BLOCK_SIZE];
    if(disk_read_block(d,sb->block_bitmap_block,bbm))return 0;
    for(u32 b=sb->first_data_block;b<sb->total_blocks;b++)
    {
        if(!bitmap_get(bbm,b))
        {
            bitmap_set(bbm,b);
            if(disk_write_block(d,sb->block_bitmap_block,bbm))return 0;
            if(disk_zero_block(d,b))return 0;
            sb->free_blocks--;
            if(save_superblock(d,sb))return 0;
            return b;
        }
    }
    fprintf(stderr,"BLock bitmap full\n");
    return 0;
}

int free_block(disk_t* d,ext2sim_superblock* sb,u32 block_no)
{
    if(!d || !sb)return -1;
    if (block_no < sb->first_data_block || block_no >= sb->total_blocks) {
        fprintf(stderr, "invalid block \n");
        return -1;
    }
    u8 bbm[EXT2SIM_BLOCK_SIZE];
    if (disk_read_block(d, sb->block_bitmap_block, bbm))return -1;

    if (!bitmap_get(bbm, block_no)) {
        fprintf(stderr, "Already free\n");
        return -1;
    }

    bitmap_clear(bbm, block_no);

    if (disk_write_block(d, sb->block_bitmap_block, bbm))return -1;

    sb->free_blocks++;
    return save_superblock(d, sb);
}

u32 alloc_inode(disk_t* d,ext2sim_superblock* sb)
{
    if(!d || !sb)return 0;
    if(sb->free_inodes==0)
    {
        fprintf(stderr,"no free inodes\n");
        return 0;
    }
    u8 ibm[EXT2SIM_BLOCK_SIZE];
    if(disk_read_block(d,sb->inode_bitmap_block,ibm))return 0;
    for(u32 i=0;i<sb->total_inodes;i++)
    {
        if(!bitmap_get(ibm,i))
        {
            bitmap_set(ibm,i);
            if(disk_write_block(d,sb->inode_bitmap_block,ibm))return 0;
            sb->free_inodes--;
            if(save_superblock(d,sb))return 0;
            return i+1;
        }
    }
    fprintf(stderr,"Inode bitmap full\n");
    return 0;
}

int free_inode(disk_t* d,ext2sim_superblock* sb,u32 inode_no)
{
    if(!d || !sb)return -1;
    if(inode_no==0 || inode_no>sb->total_inodes)
    {
        fprintf(stderr,"invalid inode\n");
        return -1;
    }   
    u8 ibm[EXT2SIM_BLOCK_SIZE];
    if(disk_read_block(d,sb->inode_bitmap_block,ibm))return -1;
    u32 idx = inode_no-1;
    if(!bitmap_get(ibm,idx))
    {
        fprintf(stderr,"Already free\n");
        return -1;
    }
    bitmap_clear(ibm,idx);
    if(disk_write_block(d,sb->inode_bitmap_block,ibm))return -1;
    sb->free_inodes++;
    return save_superblock(d,sb);
}