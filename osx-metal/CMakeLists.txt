fips_begin_lib(osxentry)
    fips_files(osxentry.m osxentry.h sokol_gfx.m)
    fips_frameworks_osx(Cocoa Metal MetalKit QuartzCore)
fips_end_lib()

fips_begin_app(clear-metal windowed)
    fips_files(clear-metal.c)
    fips_deps(osxentry)
fips_end_app()
