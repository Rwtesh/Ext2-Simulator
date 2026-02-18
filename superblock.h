#ifndef SUPERBLOCK_H
#define SUPERBLOCK_H

#include <stdint.h>
#include <stddef.h>

#define FS_MAGIC 0x4D455832u

typedef struct Superblock
{
  uint32_t magic;
  uint32_t version;

  uint32_t block_size;
  uint32_t total_blocks;
  uint32_t total_inodes;

  uint32_t inode_table_start;
  uint32_t inode_table_blocks;
  uint32_t inode_bitmap_block;
  uint32_t block_bitmap_block;

  uint32_t root_dir_block;
  uint32_t data_block_start;

  uint32_t free_inode_count;
  uint32_t free_block_count;

}Superblock;

int superblock_init_fresh(Superblock* sb, size_t total_blocks, size_t block_size, size_t total_inodes);
int superblock_load(Superblock* sb);
int superblock_flush(Superblock* sb);

#endif
