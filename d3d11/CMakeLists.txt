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