#pragma once
/*
    Simple file access functions.
*/
extern void fs_init(void);
extern bool fs_load_file(const char* path);
extern void fs_load_mem(const uint8_t* ptr, uint32_t size);
extern uint32_t fs_size(void);
extern const uint8_t* fs_ptr(void);
extern void fs_free(void);

/*== IMPLEMENTATION ==========================================================*/
#ifdef COMMON_IMPL
#include <stdlib.h>
#include <string.h>
#if !defined(__EMSCRIPTEN__)
#include <stdio.h>
#else
#include <emscripten/emscripten.h>
#endif

#define FS_MAX_SIZE (512 * 1024)
typedef struct {
    uint8_t* ptr;
    uint32_t size;
    uint8_t buf[FS_MAX_SIZE];
} fs_state;
static fs_state fs;

void fs_free(void) {
    fs.ptr = 0;
    fs.size = 0;
}

void fs_load_mem(const uint8_t* ptr, uint32_t size) {
    fs_free();
    if ((size > 0) && (size <= FS_MAX_SIZE)) {
        fs.size = size;
        fs.ptr = fs.buf;
        memcpy(fs.ptr, ptr, size);
    }
}

#if !defined(__EMSCRIPTEN__)
bool fs_load_file(const char* path) {
    fs_free();
    FILE* fp = fopen(path, "rb");
    if (fp) {
        fseek(fp, 0, SEEK_END);
        int size = ftell(fp);
        if (size <= FS_MAX_SIZE) {
            fs.size = size;
            fseek(fp, 0, SEEK_SET);
            if (fs.size > 0) {
                fs.ptr = fs.buf;
                fread(fs.ptr, 1, fs.size, fp);
            }
            fclose(fp);
            return true;
        }
    }
    return false;
}
#else
EMSCRIPTEN_KEEPALIVE void emsc_load_data(const uint8_t* ptr, int size) {
    fs_load_mem(ptr, size);
}

EM_JS(void, emsc_load_file, (const char* path_cstr), {
    var path = Pointer_stringify(path_cstr);
    var req = new XMLHttpRequest();
    req.open("GET", path);
    req.responseType = "arraybuffer";
    req.onload = function(e) {
        var uint8Array = new Uint8Array(req.response);
        var res = Module.ccall('emsc_load_data',
            'int',
            ['array', 'number'],
            [uint8Array, uint8Array.length]);
    };
    req.send();
});

/* NOTE: this is loading the data asynchronously, need to check fs_ptr()
   whether the data has actually been loaded!
*/
bool fs_load_file(const char* path) {
    fs_free();
    emsc_load_file(path);
    return false;
}
#endif

void fs_init(void) {
    fs.ptr = 0;
    fs.size = 0;
    memset(fs.buf, 0, sizeof(fs.buf));
}

const uint8_t* fs_ptr(void) {
    return fs.ptr;
}

uint32_t fs_size(void) {
    return fs.size;
}

#endif /* COMMON_IMPL */
