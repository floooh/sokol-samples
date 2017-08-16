fips_begin_lib(osxentry)
    fips_files(osxentry.m osxentry.h sokol_gfx.m)
    fips_frameworks_osx(Cocoa Metal MetalKit QuartzCore)
fips_end_lib()

fips_begin_app(clear-metal windowed)
    fips_files(clear-metal.c)
    fips_deps(osxentry)
fips_end_app()

fips_begin_app(triangle-metal windowed)
    fips_files(triangle-metal.c)
    fips_deps(osxentry)
fips_end_app()

fips_begin_app(quad-metal windowed)
    fips_files(quad-metal.c)
    fips_deps(osxentry)
fips_end_app()

fips_begin_app(cube-metal windowed)
    fips_files(cube-metal.c)
    fips_deps(osxentry)
fips_end_app()

fips_begin_app(texcube-metal windowed)
    fips_files(texcube-metal.c)
    fips_deps(osxentry)
fips_end_app()
