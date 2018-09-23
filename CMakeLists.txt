#
# project: sokol-samples
#
cmake_minimum_required(VERSION 2.8)
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)

# include the fips main cmake file
get_filename_component(FIPS_ROOT_DIR "../fips" ABSOLUTE)
include("${FIPS_ROOT_DIR}/cmake/fips.cmake")

fips_setup(PROJECT sokol-samples)
fips_ide_group("Imports")
if (NOT USE_SOKOL_APP AND NOT SOKOL_USE_D3D11 AND NOT SOKOL_USE_METAL)
    fips_import_fips_glfw_glfw3()
endif()
fips_import_fips_imgui_imgui()
fips_import_fips_libmodplug_libmodplug()
fips_ide_group("")

# just suppress this pesky "missing field initializer warning" for now
if (FIPS_CLANG OR FIPS_GCC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-missing-field-initializers")
endif()
if (FIPS_GCC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-missing-braces")
endif()

if (USE_SOKOL_APP)
    add_subdirectory(sapp)
else()
    if (FIPS_EMSCRIPTEN)
        add_subdirectory(html5)
    elseif (FIPS_RASPBERRYPI)
        add_subdirectory(raspi)
    elseif (SOKOL_USE_D3D11)
        add_subdirectory(d3d11)
    elseif (SOKOL_USE_METAL)
        add_subdirectory(metal)
    else()
        add_subdirectory(glfw)
    endif()
endif()

fips_finish()


