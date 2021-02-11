# select the 3D backend for sokol_gfx.h and sokol_app.h
# the sokol_config() macro is in the top-level CMakeLists.txt file!
sokol_config()

#[[fips_ide_group(Samples/imgui)
fips_begin_app(imgui-sapp windowed)
    fips_vs_warning_level(3)
    fips_files(imgui-sapp.cc)
    # use sokol-cpp for testing here, this isn't required though
    fips_deps(sokol-cpp imgui)
    if (FIPS_IOS)
        fips_files(ios-info.plist)
    endif()
fips_end_app()
]]
fips_begin_app(imgui-highdpi-sapp windowed)
    fips_vs_warning_level(3)
    fips_files(imgui-highdpi-sapp.cc)
    fips_deps(sokol imgui)
    if (FIPS_IOS)
        fips_files(ios-info.plist)
    endif()
fips_end_app()

#fips_begin_app(imgui-usercallback-sapp windowed)
#    fips_vs_warning_level(3)
#    fips_files(imgui-usercallback-sapp.c)
#    sokol_shader(imgui-usercallback-sapp.glsl ${slang})
#    fips_deps(sokol cimgui)
#    if (FIPS_IOS)
#        fips_files(ios-info.plist)
#    endif()
#fips_end_app()

#fips_begin_app(imgui-perf-sapp windowed)
#    fips_vs_warning_level(3)
#    fips_files(imgui-perf-sapp.c)
#    fips_deps(sokol cimgui)
#    if (FIPS_IOS)
#        fips_files(ios-info.plist)
#    endif()
#fips_end_app()
#[[
fips_ide_group(Samples/events)
fips_begin_app(events-sapp windowed)
    fips_vs_warning_level(3)
    fips_files(events-sapp.cc)
    fips_deps(sokol imgui)
    if (FIPS_IOS)
        fips_files(ios-info.plist)
    endif()
fips_end_app()
]]

