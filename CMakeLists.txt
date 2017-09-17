#
# project: sokol-samples
#
cmake_minimum_required(VERSION 2.8)

# include the fips main cmake file
get_filename_component(FIPS_ROOT_DIR "../fips" ABSOLUTE)
include("${FIPS_ROOT_DIR}/cmake/fips.cmake")

fips_setup(PROJECT sokol-samples)
# just suppress this pesky "missing field initializer warning" for now
if (FIPS_CLANG OR FIPS_GCC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-missing-field-initializers")
endif()
if (FIPS_EMSCRIPTEN)
    add_subdirectory(html5)
elseif (SOKOL_USE_D3D11)
    add_subdirectory(d3d11)
elseif (SOKOL_USE_METAL) 
    add_subdirectory(osx-metal)
else()
    add_subdirectory(glfw)
endif()

fips_finish()


