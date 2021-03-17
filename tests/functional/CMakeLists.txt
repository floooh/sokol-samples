fips_begin_app(sokol-test cmdline)
    fips_vs_warning_level(4)
    fips_files(
        sokol_args_test.c
        sokol_audio_test.c
        sokol_debugtext_test.c
        sokol_fetch_test.c
        sokol_gfx_test.c
        sokol_gl_test.c
        sokol_shape_test.c
        sokol_test.c
    )
    fips_dir(assets)
    fipsutil_copy(sokol-fetch-test.yml)
fips_end_app()
