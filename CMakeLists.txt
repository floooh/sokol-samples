#
# project: sokol-samples
#
cmake_minimum_required(VERSION 2.8)

# include the fips main cmake file
get_filename_component(FIPS_ROOT_DIR "../fips" ABSOLUTE)
include("${FIPS_ROOT_DIR}/cmake/fips.cmake")

fips_setup(PROJECT sokol-samples)
if (FIPS_EMSCRIPTEN)
    add_subdirectory(html5)
else()
    add_subdirectory(glfw)
endif()
fips_finish()


