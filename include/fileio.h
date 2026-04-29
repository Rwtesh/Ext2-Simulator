#ifndef FILEIO_H
#define FILEIO_H

#include "types.h"

// Overwrite file content with exactly len bytes from data.
// Returns 0 on success, -1 on error.
int cmd_write(const char* imgPath, const char* absPath, const void* data, u32 len);

// Print file content to stdout.
// Returns 0 on success, -1 on error.
int cmd_cat(const char* imgPath, const char* absPath);

#endif