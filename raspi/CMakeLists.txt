# hack for GCC 4.9
fips_begin_lib(raspientry)
    fips_files(raspientry.c raspientry.h)
fips_end_lib()
target_include_directories(raspientry PUBLIC
    "/opt/vc/include"
    "/opt/vc/include/interface/vcos/pthreads"
    "/opt/vc/include/interface/vmcs_host/linux"
)
link_directories("/opt/vc/lib")

fips_begin_app(clear-raspi windowed)
    fips_files(clear-raspi.c)
    fips_deps(raspientry)
fips_end_app()
