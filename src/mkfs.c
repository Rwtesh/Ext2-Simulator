#include "../include/ext2sim.h"
#include "../include/disk.h"
#include "../include/mkfs.h"
#include "../include/types.h"
#include "../include/dirent.h"

#include<stdlib.h>
#include<stdio.h>
#include<string.h>

static u32 ceil_div(u32 a,u32 b)
{
    return (a+b-1)/b;
}
static void bitmap_set(u8* bmp,u32 idx0)
{
    bmp[idx0/8] |= (u8)(1u << (idx0%8));
}

int cmd_mkfs(const char* img_path,u32 size_mb)
{
    // printf("hello1\n");
    const u32 block_size = EXT2SIM_BLOCK_SIZE;
    const u32 total_bytes = size_mb * 1024u * 1024u;
    const u32 total_blocks = total_bytes/block_size;

    const u32 total_inodes = 1024;
    const u32 itable_bytes = total_inodes*(u32)sizeof(ext2sim_inode);
    const u32 itable_blocks = ceil_div(itable_bytes,block_size);

    const u32 SUPERBLOCK_BLK = 1;
    const u32 GD_BLK = 2;
    const u32 BBM_BLK = 3;
    const u32 IBM_BLK = 4;
    const u32 ITABLE_BLK = 5;

    const u32 FIRST_DATA_BLK = ITABLE_BLK + itable_blocks;
    //
    printf("total_bytes=%u total_blocks=%u itable_blocks=%u FIRST_DATA_BLK=%u\n",
       (unsigned)total_bytes, (unsigned)total_blocks,
       (unsigned)itable_blocks, (unsigned)FIRST_DATA_BLK);

if (total_blocks <= FIRST_DATA_BLK) {
    fprintf(stderr, "mkfs: image too small for metadata layout\n");
    return -1;
}

    if(total_blocks<=FIRST_DATA_BLK)return -1;

    FILE* fp = fopen(img_path,"wb");
    if(!fp)return -1;

    if(fseek(fp,(long)total_bytes-1,SEEK_SET))
    {
        fclose(fp);
        return -1;
    }

    if(fputc(0,fp)==EOF)    //didn't use memset, reason too long: look it up
    {
        fclose(fp);
        return -1;
    }
    fclose(fp);
//     FILE *fp = fopen(img_path, "wb");
// if (!fp) {
//     perror("mkfs: fopen(wb)");
//     return -1;
// }

// if (fseek(fp, (long)total_bytes - 1, SEEK_SET) != 0) {
//     perror("mkfs: fseek");
//     fclose(fp);
//     return -1;
// }

// if (fputc(0, fp) == EOF) {
//     perror("mkfs: fputc");
//     fclose(fp);
//     return -1;
// }you

// if (fclose(fp) != 0) {
//     perror("mkfs: fclose");
//     return -1;
// }

// printf("hello2\n");


    disk_t d;
    if(disk_open(&d,img_path,"r+b",block_size))return -1;

    ext2sim_superblock sb;
    memset(&sb,0,sizeof(sb));   //So that the pad ins't garbage
    sb.magic=EXT2SIM_MAGIC;
    sb.block_size=block_size;
    sb.total_blocks=total_blocks;
    sb.total_inodes=total_inodes;

    sb.block_bitmap_block=BBM_BLK;
    sb.inode_bitmap_block=IBM_BLK;
    sb.inode_table_block=ITABLE_BLK;
    sb.first_data_block=FIRST_DATA_BLK;

    //Root directory uses FIRST_DATA_BLK as its directory block
    const u32 ROOT_DIR_BLK = FIRST_DATA_BLK;

    //NOTE: we must count ROOT_DIR_BLK as used, otherwise alloc_block() may hand it out again
    u32 used_blocks=FIRST_DATA_BLK+1;
    sb.free_blocks=total_blocks-used_blocks;

    u32 used_inodes=2;
    sb.free_inodes=total_inodes-used_inodes;

    if(disk_write_block(&d,SUPERBLOCK_BLK,&sb))
    {
        disk_close(&d);
        return -1;
    }
        // printf("hello3\n");

    ext2sim_group_desc gd;
    memset(&gd,0,sizeof(gd));
    gd.block_bitmap_block=BBM_BLK;
    gd.inode_bitmap_block=IBM_BLK;
    gd.inode_table_block=ITABLE_BLK;
    
    u8 gd_block[EXT2SIM_BLOCK_SIZE];
    memset(gd_block, 0, sizeof(gd_block));
    memcpy(gd_block, &gd, sizeof(gd));

    if (disk_write_block(&d, GD_BLK, gd_block)) 
    {
        disk_close(&d);
        return -1;
    }
    // printf("hello4\n");

    u8 bbm[EXT2SIM_BLOCK_SIZE];
    memset(bbm,0,sizeof(bbm));
    for(u32 i=0;i<FIRST_DATA_BLK;i++)
        bitmap_set(bbm,i);

    //mark ROOT_DIR_BLK as used too
    bitmap_set(bbm,ROOT_DIR_BLK);

    if(disk_write_block(&d,BBM_BLK,bbm))
    {
        disk_close(&d);
        return -1;
    }
    // printf("hello5\n");

    u8 ibm[EXT2SIM_BLOCK_SIZE];
    memset(ibm,0,sizeof(ibm));
    bitmap_set(ibm,0);
    bitmap_set(ibm,EXT2SIM_ROOT_INODE-1);//just 1
    if(disk_write_block(&d,IBM_BLK,ibm))
    {
        disk_close(&d);
        return -1;
    }

    //this is kinda defensive, we have already set all the bytes to 0 earlier so this is redundant/future proof:
    for(u32 i=0;i<itable_blocks;i++)
    {
        if(disk_zero_block(&d,ITABLE_BLK+i))
        {
            disk_close(&d);
            return -1;
        }
    }
    // printf("hello6\n");

    //initialize root directory block with "." and ".."
    u8 root_dir_block[EXT2SIM_BLOCK_SIZE];
    memset(root_dir_block,0,sizeof(root_dir_block));

    ext2sim_dirent* dent = (ext2sim_dirent*)root_dir_block;

    dent[0].inode = EXT2SIM_ROOT_INODE;
    memset(dent[0].name,0,sizeof(dent[0].name));
    strncpy((char*)dent[0].name,".",EXT2SIM_NAME_MAX);
    dent[0].name[EXT2SIM_NAME_MAX]='\0';

    dent[1].inode = EXT2SIM_ROOT_INODE;
    memset(dent[1].name,0,sizeof(dent[1].name));
    strncpy((char*)dent[1].name,"..",EXT2SIM_NAME_MAX);
    dent[1].name[EXT2SIM_NAME_MAX]='\0';

    if(disk_write_block(&d,ROOT_DIR_BLK,root_dir_block))
    {
        disk_close(&d);
        return -1;
    }

    ext2sim_inode root;
    memset(&root,0,sizeof(root));
    root.mode=0x4000;
    root.links_count=2;
    root.size=block_size;
    root.direct[0]=ROOT_DIR_BLK;
    
    u32 ino = EXT2SIM_ROOT_INODE;
    u32 idx0 = ino - 1;
    
    u32 offset = idx0 * (u32)sizeof(ext2sim_inode);

    u32 blk = ITABLE_BLK + (offset/block_size);
    u32 off = offset%block_size;

    u8 itable_block[EXT2SIM_BLOCK_SIZE];
    if(disk_read_block(&d,blk,itable_block))
    {
        disk_close(&d);
        return -1;
    }
    memcpy(itable_block+off,&root,sizeof(root));
    if(disk_write_block(&d,blk,itable_block))
    {
        disk_close(&d);
        return -1;
    }
        // printf("hello7\n");

    disk_close(&d);
    return 0;
}