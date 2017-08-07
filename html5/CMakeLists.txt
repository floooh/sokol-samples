fips_begin_app(clear-emsc windowed)
    fips_files(clear-emsc.c)
fips_end_app()

fips_begin_app(triangle-emsc windowed)
    fips_files(triangle-emsc.c)
fips_end_app()

fips_begin_app(quad-emsc windowed)
    fips_files(quad-emsc.c)
fips_end_app()

fips_begin_app(cube-emsc windowed)
    fips_files(cube-emsc.c)
fips_end_app()

fips_begin_app(texcube-emsc windowed)
    fips_files(texcube-emsc.c)
fips_end_app()

fips_begin_app(instancing-emsc windowed)
    fips_files(instancing-emsc.c)
fips_end_app()

fips_begin_app(offscreen-emsc windowed)
    fips_files(offscreen-emsc.c)
fips_end_app()

fips_begin_app(mrt-emsc windowed)
    fips_files(mrt-emsc.c)
fips_end_app()

fips_begin_app(imgui-emsc windowed)
    fips_files(imgui-emsc.cc)
    fips_deps(imgui)
fips_end_app()