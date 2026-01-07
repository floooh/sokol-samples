fips_begin_lib(d3d12entry)
    fips_files(d3d12entry.c d3d12entry.h)
fips_end_lib()

fips_begin_app(clear-d3d12 windowed)
    fips_files(clear-d3d12.c)
    fips_deps(d3d12entry)
fips_end_app()

fips_begin_app(triangle-d3d12 windowed)
    fips_files(triangle-d3d12.c)
    fips_deps(d3d12entry)
fips_end_app()

fips_begin_app(quad-d3d12 windowed)
    fips_files(quad-d3d12.c)
    fips_deps(d3d12entry)
fips_end_app()

fips_begin_app(cube-d3d12 windowed)
    fips_files(cube-d3d12.c)
    fips_deps(d3d12entry)
fips_end_app()

fips_begin_app(texcube-d3d12 windowed)
    fips_files(texcube-d3d12.c)
    fips_deps(d3d12entry)
fips_end_app()

fips_begin_app(instancing-d3d12 windowed)
    fips_files(instancing-d3d12.c)
    fips_deps(d3d12entry)
fips_end_app()

fips_begin_app(instancing-compute-d3d12 windowed)
    fips_files(instancing-compute-d3d12.c)
    fips_deps(d3d12entry)
fips_end_app()

fips_begin_app(offscreen-d3d12 windowed)
    fips_files(offscreen-d3d12.c)
    fips_deps(d3d12entry)
fips_end_app()

fips_begin_app(mrt-d3d12 windowed)
    fips_files(mrt-d3d12.c)
    fips_deps(d3d12entry)
fips_end_app()

fips_begin_app(arraytex-d3d12 windowed)
    fips_files(arraytex-d3d12.c)
    fips_deps(d3d12entry)
fips_end_app()

fips_begin_app(dyntex-d3d12 windowed)
    fips_files(dyntex-d3d12.c)
    fips_deps(d3d12entry)
fips_end_app()

fips_begin_app(binshader-d3d12 windowed)
    fips_files(binshader-d3d12.c binshader_vs.h binshader_fs.h)
    fips_deps(d3d12entry)
fips_end_app()

fips_begin_app(imgui-d3d12 windowed)
    fips_files(imgui-d3d12.cc)
    fips_deps(d3d12entry imgui)
fips_end_app()

fips_begin_app(mipmap-d3d12 windowed)
    fips_files(mipmap-d3d12.c)
    fips_deps(d3d12entry)
fips_end_app()

fips_begin_app(blend-d3d12 windowed)
    fips_files(blend-d3d12.c)
    fips_deps(d3d12entry)
fips_end_app()

fips_begin_app(inject-d3d12 windowed)
    fips_files(inject-d3d12.c)
    fips_deps(d3d12entry)
fips_end_app()

fips_begin_app(bufferoffsets-d3d12 windowed)
    fips_files(bufferoffsets-d3d12.c)
    fips_deps(d3d12entry)
fips_end_app()

fips_begin_app(vertexpulling-d3d12 windowed)
    fips_files(vertexpulling-d3d12.c)
    fips_deps(d3d12entry)
fips_end_app()
