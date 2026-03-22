#include "../include/info.h"
#include "../include/disk.h"
#include "../include/ext2sim.h"
#include "../include/types.h"

#include <stdio.h>
#include <string.h>

int cmd_info(const char* img_path)
{
    disk_t d;
    if(disk_open(&d,img_path,"rb",EXT2SIM_BLOCK_SIZE))
    {
        fprintf(stderr, "failed to open image '%s'\n", img_path);
        disk_close(&d);
        return -1;
    }

    u8 sb_block[EXT2SIM_BLOCK_SIZE];
    if(disk_read_block(&d,1,sb_block))
    {
        fprintf(stderr, "failed to read superblock\n");
        disk_close(&d);
        return -1;
    }

    ext2sim_superblock sb;
    memset(&sb,0,sizeof(sb));
    memcpy(&sb,sb_block,sizeof(sb));

    if(sb.magic!=EXT2SIM_MAGIC)
    {
        fprintf(stderr,"magic verification failed\n");
        disk_close(&d);
        return -1;
    }
    printf("ext2sim info for %s\n", img_path);
    printf("magic: 0x%08X\n", (unsigned)sb.magic);
    printf("block_size: %u\n", (unsigned)sb.block_size);
    printf("total_blocks: %u\n", (unsigned)sb.total_blocks);
    printf("total_inodes: %u\n", (unsigned)sb.total_inodes);
    printf("free_blocks: %u\n", (unsigned)sb.free_blocks);
    printf("free_inodes: %u\n", (unsigned)sb.free_inodes);
    printf("first_data_block: %u\n", (unsigned)sb.first_data_block);
    printf("block_bitmap_block: %u\n", (unsigned)sb.block_bitmap_block);
    printf("inode_bitmap_block: %u\n", (unsigned)sb.inode_bitmap_block);
    printf("inode_table_block: %u\n", (unsigned)sb.inode_table_block);

    u8 gd_block[EXT2SIM_BLOCK_SIZE];
    if(disk_read_block(&d,2,gd_block))
    {
        fprintf(stderr, "failed to read group descriptor table\n");
        disk_close(&d);
        return -1;
    }

    ext2sim_group_desc gd;
    memset(&gd,0,sizeof(gd));
    memcpy(&gd,gd_block,sizeof(gd));
    printf("group_desc:\n");
    printf("block_bitmap_block: %u\n", (unsigned)gd.block_bitmap_block);
    printf("inode_bitmap_block: %u\n", (unsigned)gd.inode_bitmap_block);
    printf("inode_table_block: %u\n", (unsigned)gd.inode_table_block);

    disk_close(&d);
    return 0;
}