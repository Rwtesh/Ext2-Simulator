#include "../include/inode.h"
#include <string.h>

int inode_read(disk_t* d,const ext2sim_superblock* sb, u32 ino, ext2sim_inode* out)
{
    if(!d || !sb || !out)return -1;
    if(ino==0 || ino> sb->total_inodes)return -1;
    
    u32 idx = ino-1;
    u32 offset = idx * (u32)sizeof(ext2sim_inode);
    u32 blk = sb->inode_table_block + (offset/sb->block_size);
    u32 off = offset%sb->block_size;

    if(sizeof(ext2sim_inode)+off > sb->block_size)return -1;    //won't happen but we roll

    u8 buf[EXT2SIM_BLOCK_SIZE];

    if(disk_read_block(d,blk,buf))return -1;
    memcpy(out,buf+off,sizeof(ext2sim_inode));
    return 0;
}

int inode_write(disk_t* d, const ext2sim_superblock* sb, u32 ino, const ext2sim_inode* in)
{
    if(!d || !sb || !in)return -1;
    if(ino==0 || ino> sb->total_inodes)return -1;

    u32 idx=ino-1;
    u32 offset = idx * (u32)sizeof(ext2sim_inode);
    u32 blk = sb->inode_table_block + (offset/sb->block_size);
    u32 off = offset%sb->block_size;

    if(sizeof(ext2sim_inode)+off > sb->block_size)return -1;
    
    u8 buf[EXT2SIM_BLOCK_SIZE];
    if(disk_read_block(d,blk,buf))return -1;
    memcpy(buf+off,in,sizeof(ext2sim_inode));

    return (disk_write_block(d,blk,buf))?-1:0;
}