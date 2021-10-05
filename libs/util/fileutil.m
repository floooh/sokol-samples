// NOTE: this file is only compiled on iOS builds
#include "fileutil.h"
#include <stdio.h>
#import <Foundation/Foundation.h>

const char* fileutil_get_path(const char* filename, char* buf, size_t buf_size) {
    NSString* ns_str = [NSBundle mainBundle].resourcePath;
    snprintf(buf, buf_size, "%s/%s", [ns_str UTF8String], filename);
    // ns_str will be autoreleased
    return buf;
}
