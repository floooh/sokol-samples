macro(add_system_libs)
    if (FIPS_IOS)
        fips_frameworks_osx(UIKit AudioToolbox AVFoundation)
        if (SOKOL_USE_METAL)
            fips_frameworks_osx(Metal MetalKit)
        else()
            fips_frameworks_osx(OpenGLES GLKit)
        endif()
    elseif (FIPS_MACOS)
        fips_frameworks_osx(Cocoa QuartzCore AudioToolbox)
        if (SOKOL_USE_METAL)
            fips_frameworks_osx(Metal MetalKit)
        else()
            fips_frameworks_osx(OpenGL)
        endif()
    elseif (FIPS_ANDROID)
        fips_libs(GLESv3 EGL OpenSLES log android)
    elseif (FIPS_LINUX)
        fips_libs(X11 Xi Xcursor GL m dl asound)
    endif()
endmacro()

set(c_common_sources
    sokol_gfx.c
    sokol_time.c
    sokol_args.c
    sokol_audio.c
    sokol_debugtext.c
    sokol_gl.c
    sokol_fontstash.c
    sokol_imgui.c
    sokol_gfx_imgui.c
    sokol_shape.c
    sokol_nuklear.c
    sokol_color.c
    sokol_main.c)

set(cc_common_sources
    sokol_gfx.cc
    sokol_time.cc
    sokol_args.cc
    sokol_audio.cc
    sokol_debugtext.cc
    sokol_gl.cc
    sokol_fontstash.cc
    sokol_imgui.cc
    sokol_gfx_imgui.cc
    sokol_shape.cc
    sokol_color.cc
    sokol_main.cc)

# test compiling the implementations as C/ObjC
if (NOT FIPS_UWP)

fips_begin_app(sokol-compiletest-c windowed)
    fips_vs_warning_level(4)
    fips_files(sokol_app.c sokol_glue.c ${c_common_sources})
    if (NOT FIPS_ANDROID AND NOT FIPS_UWP)
        fips_files(sokol_fetch.c)
    endif()
    sokol_shader(shader.glsl ${slang})
    fips_deps(cimgui nuklear)
    add_system_libs()
fips_end_app()
target_compile_definitions(sokol-compiletest-c PUBLIC ${sokol_backend})
if (FIPS_OSX)
    target_compile_options(sokol-compiletest-c PUBLIC -x objective-c)
endif()

# test compiling with dummy backend
fips_begin_app(sokol-compiletest-dummy cmdline)
    fips_vs_warning_level(4)
    fips_files(${c_common_sources})
    if (NOT FIPS_ANDROID AND NOT FIPS_UWP)
        fips_files(sokol_fetch.c)
    endif()
    fips_deps(cimgui nuklear)
fips_end_app()
target_compile_definitions(sokol-compiletest-dummy PUBLIC SOKOL_DUMMY_BACKEND)

endif() # FIPS_UWP

# test compiling implementations as C++/ObjC++
fips_begin_app(sokol-compiletest-cc windowed)
    fips_vs_warning_level(4)
    fips_files(sokol_app.cc sokol_glue.cc ${cc_common_sources})
    if (NOT FIPS_ANDROID AND NOT FIPS_UWP)
        fips_files(sokol_fetch.cc)
    endif()
    sokol_shader(shader.glsl ${slang})
    fips_deps(imgui)
    add_system_libs()
fips_end_app()
target_compile_definitions(sokol-compiletest-cc PUBLIC ${sokol_backend})
if (FIPS_OSX)
    target_compile_options(sokol-compiletest-cc PUBLIC -x objective-c++)
endif()

