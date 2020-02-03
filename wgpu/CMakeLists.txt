fips_begin_app(clear-wgpu windowed)
    fips_files(clear-wgpu.c)
fips_end_app()

fips_begin_app(triangle-wgpu windowed)
    fips_files(triangle-wgpu.c)
    sokol_shader(triangle-wgpu.glsl wgpu)
fips_end_app()
