fips_begin_lib(microui)
    fips_files(microui.c microui.h)
    if (FIPS_GCC OR FIPS_CLANG)
        target_compile_options(microui PRIVATE -Wno-sign-conversion)
    endif()
    if (FIPS_CLANG)
        target_compile_options(microui PRIVATE -Wno-shorten-64-to-32)
    endif()
    if (FIPS_MSVC)
        # 4267: '=': conversion from 'size_t' to 'int', possible loss of data
        # 4244: conversion from '__int64' to 'int', possible loss of data
        # 4996: 'sprintf': This function or variable may be unsafe. Consider using sprintf_s instead.
        target_compile_options(microui PRIVATE /wd4267 /wd4244 /wd4996)
    endif()
fips_end_lib(microui)
