#pragma once
/*
    Command line / URL argument helpers. On native platforms,
    args must be of the form "key=value" without spaces!
*/
extern void args_init(int argc, char* argv[]);
extern bool args_has(const char* key);
extern const char* args_string(const char* key);
extern bool args_string_compare(const char* key, const char* val);
extern bool args_bool(const char* key);

/*== IMPLEMENTATION ==========================================================*/
#ifdef COMMON_IMPL

#include <string.h>
#include <assert.h>
#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

#define MAX_ARGS (16)
#define BUF_SIZE (64 * 1024)

typedef struct {
    int num_args;
    const char* keys[MAX_ARGS];
    const char* vals[MAX_ARGS];
    int pos;
    char buf[BUF_SIZE];
} args_state;
static args_state args;

/* add a key-value-string of the form "key=value" */
#if !defined(__EMSCRIPTEN__)
static void add_kvp(const char* kvp) {
    const char* key_ptr;
    const char* val_ptr;
    char c;
    const char* src = kvp;
    key_ptr = args.buf + args.pos;
    val_ptr = 0;
    while (0 != (c = *src++)) {
        if ((args.pos+2) < BUF_SIZE) {
            if (c == '=') {
                args.buf[args.pos++] = 0;
                val_ptr = args.buf + args.pos;
            }
            else if (c > 32) {
                args.buf[args.pos++] = c;
            }
        }
    }
    if ((args.pos+2) < BUF_SIZE) {
        args.buf[args.pos++] = 0;
        if (args.num_args < MAX_ARGS) {
            args.keys[args.num_args] = key_ptr;
            args.vals[args.num_args] = val_ptr;
            args.num_args++;
        }
    }
}
#else
/* emscripten helpers to parse browser URL args of the form "key=value" */
static void add_kvp_2(const char* key, const char* val) {
    const char* key_ptr;
    const char* val_ptr;
    char c;
    const char* src = key;
    key_ptr = args.buf + args.pos;
    while (0 != (c = *src++)) {
        if ((args.pos+2) < BUF_SIZE) {
            args.buf[args.pos++] = c;
        }
    }
    args.buf[args.pos++] = 0;
    src = val;
    val_ptr = args.buf + args.pos;
    while (0 != (c = *src++)) {
        if ((args.pos+2) < BUF_SIZE) {
            args.buf[args.pos++] = c;
        }
    }
    args.buf[args.pos++] = 0;
    if ((args.pos+2) < BUF_SIZE) {
        args.buf[args.pos++] = 0;
        if (args.num_args < MAX_ARGS) {
            args.keys[args.num_args] = key_ptr;
            args.vals[args.num_args] = val_ptr;
            args.num_args++;
        }
    }
}

EMSCRIPTEN_KEEPALIVE void args_emsc_add(const char* key, const char* val) {
    add_kvp_2(key, val);
}

EM_JS(void, args_emsc_parse_url, (), {
    var params = new URLSearchParams(window.location.search).entries();
    for (var p = params.next(); !p.done; p = params.next()) {
        var key = p.value[0];
        var val = p.value[1];
        var res = Module.ccall('args_emsc_add', 'void', ['string','string'], [key,val]);
    }
});
#endif

void args_init(int argc, char* argv[]) {
    #if !defined(__EMSCRIPTEN__)
        for (int i = 1; i < argc; i++) {
            add_kvp(argv[i]);
        }
    #else
        args_emsc_parse_url();
    #endif
}

static int find_key(const char* key) {
    assert(key);
    for (int i = 0; i < args.num_args; i++) {
        if (args.keys[i] && (0 == strcmp(args.keys[i], key))) {
            return i;
        }
    }
    return -1;
}

bool args_has(const char* key) {
    return -1 != find_key(key);
}

/* returns empty string when key not found, will never return a nullptr */
const char* args_string(const char* key) {
    if (key) {
        int i = find_key(key);
        if ((-1 != i) && (args.vals[i])) {
            return args.vals[i];
        }
    }
    return "";
}

bool args_string_compare(const char* key, const char* val) {
    if (key && val) {
        return 0 == strcmp(args_string(key), val);
    }
    else {
        return false;
    }
}

bool args_bool(const char* key) {
    if (key) {
        const char* val = args_string(key);
        assert(val);
        if (0 == strcmp(val, "true")) {
            return true;
        }
        if (0 == strcmp(val, "yes")) {
            return true;
        }
        if (0 == strcmp(val, "on")) {
            return true;
        }
    }
    return false;
}

#endif /* COMMON_IMPL */
