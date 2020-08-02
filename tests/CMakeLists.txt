sokol_config()

if (NOT FIPS_ANDROID)
fips_begin_app(sokol-test cmdline)
    fips_vs_warning_level(3)
    fips_files(
        sokol-args-test.c
        sokol-audio-test.c
        sokol-gfx-test.c
        sokol-gl-test.c
        sokol-fetch-test.c
        sokol-debugtext-test.c
        sokol-test.c
    )
    fips_dir(assets)
    fipsutil_copy(sokol-fetch-test.yml)
    if (FIPS_LINUX)
        fips_libs(m)
    endif()
fips_end_app()
endif()

if (FIPS_OSX)
    # on Apple with Metal, must compile as Objective-C++
    fips_begin_app(objcpp-compile-test cmdline)
        fips_files(objcpp-compile-test.mm)
        sokol_shader(shader.glsl ${slang})
        if (FIPS_IOS)
            if (SOKOL_USE_METAL)
                fips_frameworks_osx(UIKit Metal MetalKit AudioToolbox)
            else()
                fips_frameworks_osx(UIKit OpenGLES GLKit AudioToolbox)
            endif()
        else()
            if (SOKOL_USE_METAL)
                fips_frameworks_osx(Cocoa QuartzCore Metal MetalKit AudioToolbox)
            else()
                fips_frameworks_osx(Cocoa QuartzCore OpenGL AudioToolbox)
            endif()
        endif()
    fips_end_app()
else()
    fips_begin_app(cpp-compile-test cmdline)
        fips_vs_warning_level(4)
        fips_files(cpp-compile-test.cc)
        sokol_shader(shader.glsl ${slang})
        fips_deps(cimgui)
        if (FIPS_ANDROID)
            fips_libs(GLESv3 EGL OpenSLES log android)
        elseif (FIPS_LINUX)
            fips_libs(X11 Xi Xcursor GL m dl asound)
        endif()
    fips_end_app()
endif()
