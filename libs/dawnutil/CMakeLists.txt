fips_begin_lib(dawnutil)
    fips_files(SampleUtils.cpp SampleUtils.h)
    # hack to include dawncpp impl
    fips_dir(${DAWN_BUILD_DIR}/gen/src/dawn)
    fips_files(webgpu_cpp.cpp)
    fips_libs(dawn_proc dawn_bindings dawn_utils dawn_native dawn_wire glfw)
fips_end_lib(dawnutil)
if (FIPS_OSX)
    target_compile_definitions(dawnutil PRIVATE DAWN_ENABLE_BACKEND_METAL)
endif()
target_include_directories(dawnutil INTERFACE .)

