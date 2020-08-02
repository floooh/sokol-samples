fips_begin_lib(basisu)
    fips_files(
        basisu_transcoder.cpp basisu_transcoder.h
        basisu_sokol.cpp basisu_sokol.h
    )
fips_end_lib()
target_compile_definitions(basisu PRIVATE BASISD_SUPPORT_BC7=0)
if (FIPS_CLANG OR FIPS_GCC)
    set_target_properties(basisu PROPERTIES COMPILE_FLAGS "-Wno-unused-variable -Wno-unused-parameter -Wno-type-limits")
endif()
