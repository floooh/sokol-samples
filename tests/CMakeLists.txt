fips_begin_app(sokol-test cmdline)
    fips_vs_warning_level(3)
    fips_files(
        sokol-args-test.c 
        sokol-audio-test.c 
        sokol-gfx-test.c 
        sokol-gl-test.c 
        sokol-test.c 
    )
    if (FIPS_LINUX)
        fips_libs(m)
    endif()
fips_end_app()

if (FIPS_OSX)
    if (USE_SOKOL_APP OR SOKOL_USE_METAL)
        # ob Apple with Metal, must compile as Objective-C++
        fips_begin_app(objcpp-compile-test cmdline)
            fips_vs_warning_level(3)
            fips_files(objcpp-compile-test.mm)
            if (FIPS_IOS)
                fips_frameworks_osx(UIKit Metal MetalKit AudioToolbox)
            else()
                fips_frameworks_osx(Cocoa QuartzCore Metal MetalKit AudioToolbox)
            endif()
        fips_end_app()
    endif()
else()
    fips_begin_app(cpp-compile-test cmdline)
        fips_vs_warning_level(3)
        fips_files(cpp-compile-test.cc)
        fips_deps(cimgui)
        if (FIPS_ANDROID)
            fips_libs(GLESv3 EGL log android)
        elseif (FIPS_LINUX)
            fips_libs(X11 GL m dl asound)
        endif()
    fips_end_app()
endif()
