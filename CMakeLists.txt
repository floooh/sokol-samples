#
# project: sokol-samples
#
cmake_minimum_required(VERSION 2.8)

# include the fips main cmake file
get_filename_component(FIPS_ROOT_DIR "../fips" ABSOLUTE)
include("${FIPS_ROOT_DIR}/cmake/fips.cmake")

fips_setup(PROJECT sokol-samples)
fips_ide_group("Imports")
if (NOT SOKOL_USE_D3D11 AND NOT SOKOL_USE_METAL)
    fips_import_fips_glfw_glfw3()
endif()
fips_import_fips_imgui_imgui()
fips_ide_group("")

# just suppress this pesky "missing field initializer warning" for now
if (FIPS_CLANG OR FIPS_GCC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-missing-field-initializers")
endif()
if (FIPS_EMSCRIPTEN)
    add_subdirectory(html5)
elseif (FIPS_RASPBERRYPI)
    add_subdirectory(raspi)
elseif (SOKOL_USE_D3D11)
    add_subdirectory(d3d11)
elseif (SOKOL_USE_METAL)
    if (FIPS_IOS)
        add_definitions(-DSOKOL_USE_IOS=1)
    else()
        add_definitions(-DSOKOL_USE_MACOS=1)
    endif()
    add_subdirectory(metal)
else()
    add_subdirectory(glfw)
endif()

if (FIPS_MACOS AND SOKOL_USE_METAL)
    add_subdirectory(sapp)
endif()

fips_finish()


