fips_begin_lib(basisu)
    fips_files(sokol_basisu.cpp sokol_basisu.h)
    if (FIPS_GCC OR FIPS_CLANG)
        target_compile_options(basisu PRIVATE -Wno-unused-value -Wno-unused-variable -Wno-unused-parameter -Wno-type-limits -Wno-deprecated-builtins)
    endif()
    if (FIPS_GCC)
        target_compile_options(basisu PRIVATE -Wno-maybe-uninitialized -Wno-class-memaccess)
    endif()
    if (FIPS_CLANG)
        target_compile_options(basisu PRIVATE -Wno-deprecated-declarations)
    endif()
fips_end_lib()
