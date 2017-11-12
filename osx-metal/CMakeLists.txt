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

fips_begin_app(offscreen-metal windowed)
    fips_files(offscreen-metal.c)
    fips_deps(osxentry)
fips_end_app()

fips_begin_app(mrt-metal windowed)
    fips_files(mrt-metal.c)
    fips_deps(osxentry)
fips_end_app()

fips_begin_app(instancing-metal windowed)
    fips_files(instancing-metal.c)
    fips_deps(osxentry)
fips_end_app()

fips_begin_app(arraytex-metal windowed)
    fips_files(arraytex-metal.c)
    fips_deps(osxentry)
fips_end_app()

fips_begin_app(imgui-metal windowed)
    fips_files(imgui-metal.cc)
    fips_deps(osxentry imgui)
fips_end_app()

fips_begin_app(dyntex-metal windowed)
    fips_files(dyntex-metal.c)
    fips_deps(osxentry)
fips_end_app()

fips_begin_app(binshader-metal windowed)
    fips_files(binshader-metal.c binshader-metal.h)
    fips_deps(osxentry)
fips_end_app()

fips_begin_app(mipmap-metal windowed)
    fips_files(mipmap-metal.c)
    fips_deps(osxentry)
fips_end_app()

fips_begin_app(blend-metal windowed)
    fips_files(blend-metal.c)
    fips_deps(osxentry)
fips_end_app()

fips_begin_app(inject-metal windowed)
    fips_files(inject-metal.m)
    fips_deps(osxentry)
fips_end_app()
