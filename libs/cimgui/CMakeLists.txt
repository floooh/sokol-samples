fips_begin_lib(cimgui)
    fips_src(.)
fips_end_lib(dbgui)
target_compile_options(cimgui PUBLIC -Wno-return-type-c-linkage)
