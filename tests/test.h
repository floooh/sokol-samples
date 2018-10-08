#pragma once
/* simple test helper functions */
#include <stdio.h>
#include <stdbool.h>

#if !defined(NDEBUG)
/* debug mode, use asserts to quit right when test fails */
#include <assert.h>
#define T(x) { assert(x); _test_state.num_success++; };
#else
/* release mode, just keep track of success/fail */
#define T(x) {if(x){_test_state.num_success++;}else{_test_state.num_fail++;};}
#endif

typedef struct {
    const char* name;
    const char* test_name;
    bool verbose;
    int num_success;
    int num_fail;
    int all_success;
    int all_fail;
} _test_state_t;
static _test_state_t _test_state;

void test_begin(const char* name) {
    assert(name);
    _test_state.verbose = true;
    _test_state.name = name;
    _test_state.test_name = "none";
    _test_state.num_success = 0;
    _test_state.num_fail = 0;
    _test_state.all_success = 0;
    _test_state.all_fail = 0;
    printf("### RUNNING TEST SUITE '%s':\n\n", name);
}

void test_no_verbose(void) {
    _test_state.verbose = false;
}

static void _test_finish_previous(void) {
    if ((_test_state.num_fail + _test_state.num_success) > 0) {
        if (_test_state.num_fail == 0) {
            if (_test_state.verbose) {
                printf("    %d succeeded\n", _test_state.num_success);
            }
        }
        else {
            printf("    *** FAIL! %s: %d failed, %d succeeded\n", _test_state.test_name, _test_state.num_fail, _test_state.num_success);
        }
    }
    _test_state.all_success += _test_state.num_success;
    _test_state.all_fail += _test_state.num_fail;
}

bool test_failed(void) {
    return _test_state.num_fail > 0;
}

int test_end(void) {
    assert(_test_state.name);
    _test_finish_previous();
    if (_test_state.all_fail != 0) {
        printf("%s FAILED: %d failed, %d succeeded\n",
            _test_state.name, _test_state.all_fail, _test_state.all_success);
        return 10;
    }
    else {
        printf("%s SUCCESS: %d tests succeeded\n", _test_state.name, _test_state.all_success);
        return 0;
    }
}

void test(const char* name) {
    assert(name);
    assert(_test_state.name);
    _test_finish_previous();
    if (_test_state.verbose) {
        printf("  >> %s:\n", name);
    }
    _test_state.test_name = name;
    _test_state.num_success = 0;
    _test_state.num_fail = 0;
}
