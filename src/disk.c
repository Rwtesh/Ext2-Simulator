#include "../include/disk.h"
#include <string.h>

//static so that its only visible in this file(not part of public api),its just a helper fn anw:
static int seek_block(disk_t* d,u32 block_no)
{
    long offset = (long)block_no * (long)d->block_size;
    return fseek(d->fp,offset,SEEK_SET);
}

int disk_open(disk_t* d,const char* path,const char* mode,u32 block_size)
{
    d->fp = fopen(path,mode);
    if(!d->fp)return -1;
    d->block_size=block_size;
    return 0;
}
int disk_close(disk_t* d)
{
    if(!d || !d->fp)return 0;
    int returnCode=fclose(d->fp);
    d->fp=NULL;
    return (returnCode==0)?0:-1;
}

int disk_read_block(disk_t* d, u32 block_no,void* buf)
{
    if(!d || !d->fp)return -1;
    if(seek_block(d,block_no))return -1;

    size_t n=fread(buf,1,d->block_size,d->fp);
    return (n==d->block_size)?0:-1;
}
int disk_write_block(disk_t* d,u32 block_no, const void* buf)
{
    if(!d || !d->fp)return -1;
    if(seek_block(d,block_no))return -1;

    size_t n=fwrite(buf,1,d->block_size,d->fp);
    return(n==d->block_size)?0:-1;
}

int disk_zero_block(disk_t* d,u32 block_no)
{
    if(!d || !d->fp)return -1;
    unsigned char buf[1024];
    memset(buf,0,d->block_size);
    return disk_write_block(d,block_no,buf);
}