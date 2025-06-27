fips_begin_lib(nuklear)
    fips_files(nuklear.c nuklear.h)
    if (FIPS_MSVC)
        # 5287: operands are different enum types
        target_compile_options(nuklear PUBLIC /wd5287)
    endif()
fips_end_lib()
