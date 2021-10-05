// NOTE: this file is only compiled on non-iOS builds
#include "fileutil.h"
#include <stdio.h>

const char* fileutil_get_path(const char* filename, char* buf, size_t buf_size) {
    snprintf(buf, buf_size, "%s", filename);
    return buf;
}
