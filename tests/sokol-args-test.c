//------------------------------------------------------------------------------
//  sokol-args-test.c
//------------------------------------------------------------------------------
#define SOKOL_IMPL
#include "sokol_args.h"
#include "test.h"

static int argc_1 = 4;
static const char* argv_1[] = { "exename", "kvp0=val0", "kvp1=val1", "kvp2=val2" };

static void test_init_shutdown(void) {
    test("sokol-args init shutdown");
    sargs_setup(&(sargs_desc){
        .argc = argc_1,
        .argv = argv_1,
    });
    T(sargs_isvalid());
    T(_sargs.max_args == _SARGS_MAX_ARGS_DEF);
    T(_sargs.num_args == 3);
    T(_sargs.args);
    T(_sargs.buf_size == _SARGS_BUF_SIZE_DEF);
    T(_sargs.buf_pos == 31);
    T(_sargs.buf);
    sargs_shutdown();
    T(!sargs_isvalid());
    T(0 == _sargs.args);
    T(0 == _sargs.buf);
}

int main() {
    test_begin("sokol-args-test");
    test_init_shutdown();
    return test_end();
}

