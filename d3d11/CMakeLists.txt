fips_begin_lib(d3d11entry)
    fips_files(d3d11entry.c d3d11entry.h)
fips_end_lib()

fips_begin_app(clear-d3d11 windowed)
    fips_files(clear-d3d11.c)
    fips_deps(d3d11entry)
fips_end_app()

fips_begin_app(triangle-d3d11 windowed)
    fips_files(triangle-d3d11.c)
    fips_deps(d3d11entry)
fips_end_app()

fips_begin_app(quad-d3d11 windowed)
    fips_files(quad-d3d11.c)
    fips_deps(d3d11entry)
fips_end_app()

fips_begin_app(cube-d3d11 windowed)
    fips_files(cube-d3d11.c)
    fips_deps(d3d11entry)
fips_end_app()

fips_begin_app(texcube-d3d11 windowed)
    fips_files(texcube-d3d11.c)
    fips_deps(d3d11entry)
fips_end_app()

fips_begin_app(instancing-d3d11 windowed)
    fips_files(instancing-d3d11.c)
    fips_deps(d3d11entry)
fips_end_app()

fips_begin_app(offscreen-d3d11 windowed)
    fips_files(offscreen-d3d11.c)
    fips_deps(d3d11entry)
fips_end_app()

fips_begin_app(mrt-d3d11 windowed)
    fips_files(mrt-d3d11.c)
    fips_deps(d3d11entry)
fips_end_app()

fips_begin_app(arraytex-d3d11 windowed)
    fips_files(arraytex-d3d11.c)
    fips_deps(d3d11entry)
fips_end_app()

fips_begin_app(dyntex-d3d11 windowed)
    fips_files(dyntex-d3d11.c)
    fips_deps(d3d11entry)
fips_end_app()

fips_begin_app(binshader-d3d11 windowed)
    fips_files(binshader-d3d11.c binshader_vs.h binshader_fs.h)
    fips_deps(d3d11entry)
fips_end_app()

fips_begin_app(imgui-d3d11 windowed)
    fips_files(imgui-d3d11.cc)
    fips_deps(d3d11entry imgui)
fips_end_app()

fips_begin_app(mipmap-d3d11 windowed)
    fips_files(mipmap-d3d11.c)
    fips_deps(d3d11entry)
fips_end_app()

fips_begin_app(blend-d3d11 windowed)
    fips_files(blend-d3d11.c)
    fips_deps(d3d11entry)
fips_end_app()

fips_begin_app(inject-d3d11 windowed)
    fips_files(inject-d3d11.c)
    fips_deps(d3d11entry)
fips_end_app()

fips_begin_app(bufferoffsets-d3d11 windowed)
    fips_files(bufferoffsets-d3d11.c)
    fips_deps(d3d11entry)
fips_end_app()