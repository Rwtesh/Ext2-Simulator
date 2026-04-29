#ifndef TOUCH_H
#define TOUCH_H

#include "types.h"

// Create file if it doesn't exist (absolute paths only).
// Returns 0 on success, -1 on error.
int cmdTouch(const char* imgPath, const char* absPath);

#endif