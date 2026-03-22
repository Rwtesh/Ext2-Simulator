#ifndef DISK_H
#define DISK_H

#include<stdio.h>
#include<stdint.h>
#include "types.h"

typedef struct 
{
    FILE* fp;
    u32 block_size;
}disk_t;

int disk_open(disk_t* d,const char* path,const char* mode,u32 block_size);
int disk_close(disk_t* d);
int disk_read_block(disk_t* d, u32 block_no,void* buf);
int disk_write_block(disk_t* d,u32 block_no,const void* buf);

//For convinience:
int disk_zero_block(disk_t* d,u32 block_no);

#endif