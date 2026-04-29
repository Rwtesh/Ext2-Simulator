#ifndef EXT2SIM_H
#define EXT2SIM_H

#include <stdint.h>
#include "types.h"

#define EXT2SIM_MAGIC 0xabcd1234u 
#define EXT2SIM_BLOCK_SIZE 1024
#define EXT2SIM_ROOT_INODE 2

#define EXT2SIM_MT  0xF000
#define EXT2SIM_DIR 0x4000
#define EXT2SIM_REG 0x8000

typedef struct
{
    u32 magic;
    u32 block_size;
    u32 total_blocks;
    u32 total_inodes;
    u32 free_blocks;
    u32 free_inodes;
    u32 first_data_block;
    u32 block_bitmap_block;
    u32 inode_bitmap_block;
    u32 inode_table_block;
    
    
    //padding to make sure that the size of superblock is 1024:
    u8 _pad[EXT2SIM_BLOCK_SIZE-(10*4)];
}ext2sim_superblock;


//Redundant for now(since we are only keeping one block group)/keeping it for later expansion and as a mental model:
typedef struct
{
    u32 block_bitmap_block;
    u32 inode_bitmap_block;
    u32 inode_table_block;
    u32 reserved;
}ext2sim_group_desc;


typedef struct
{
    u16 mode;
    u16 links_count;
    u32 size;
    u32 direct[12];
    u8 reserved[64]; 
}ext2sim_inode;


#endif