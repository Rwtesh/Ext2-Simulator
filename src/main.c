#include "../include/mkfs.h"
#include "../include/info.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

static void helper(const char* arg)
{
    fprintf(stderr,"Usage:\n"
        "   %s mkfs <image_path> <size in mb>\n"
        "   %s info <image_path>\n",arg,arg);
}

int main(int argc,char** argv)
{
    if(argc<2)
    {
        helper(argv[0]);
    }
    if(strcmp(argv[1],"mkfs")==0)
    {
        if(argc != 4)
        {
            helper(argv[0]);
            return 1;
        }
        const char* img = argv[2];
        errno = 0;
        char* end=NULL;
        unsigned long sizeMb=strtoul(argv[3],&end,10);
        if(errno!=0 || end == argv[3] || *end!='\0' ||sizeMb==0 || sizeMb>8)
        {
            fprintf(stderr, "wrong size\n");
        }
        int rc = cmd_mkfs(img, (u32)sizeMb);
        if (rc != 0) fprintf(stderr, "mkfs failed\n");
        return (rc == 0) ? 0 : 1;
    }
    if(strcmp(argv[1],"info")==0)
    {
        if (argc != 3) {
            helper(argv[0]);
            return 1;
        }

        const char *img = argv[2];
        int rc = cmd_info(img);
        return (rc == 0) ? 0 : 1;
    }
    helper(argv[0]);
    return 1;
}
