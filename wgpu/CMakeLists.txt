fips_begin_lib(wgpu_entry)
    fips_files(wgpu_entry.c wgpu_entry_swapchain.c wgpu_entry.h)
    if (FIPS_EMSCRIPTEN)
        fips_files(wgpu_entry_emsc.c)
    else()
        fips_files(wgpu_entry_dawn.cc)
        fips_libs(webgpu_dawn webgpu_glfw)
    endif()
fips_end_lib()

fips_begin_app(clear-wgpu windowed)
    fips_files(clear-wgpu.c)
    fips_deps(wgpu_entry)
fips_end_app()

fips_begin_app(triangle-wgpu windowed)
    fips_files(triangle-wgpu.c)
    fips_deps(wgpu_entry)
fips_end_app()

fips_begin_app(quad-wgpu windowed)
    fips_files(quad-wgpu.c)
    fips_deps(wgpu_entry)
fips_end_app()

fips_begin_app(bufferoffsets-wgpu windowed)
    fips_files(bufferoffsets-wgpu.c)
    fips_deps(wgpu_entry)
fips_end_app()

fips_begin_app(cube-wgpu windowed)
    fips_files(cube-wgpu.c)
    fips_deps(wgpu_entry)
fips_end_app()

fips_begin_app(noninterleaved-wgpu windowed)
    fips_files(noninterleaved-wgpu.c)
    fips_deps(wgpu_entry)
fips_end_app()

fips_begin_app(texcube-wgpu windowed)
    fips_files(texcube-wgpu.c)
    fips_deps(wgpu_entry)
fips_end_app()

fips_begin_app(offscreen-wgpu windowed)
    fips_files(offscreen-wgpu.c)
    fips_deps(wgpu_entry)
fips_end_app()

fips_begin_app(instancing-wgpu windowed)
    fips_files(instancing-wgpu.c)
    fips_deps(wgpu_entry)
fips_end_app()

fips_begin_app(mrt-wgpu windowed)
    fips_files(mrt-wgpu.c)
    fips_deps(wgpu_entry)
fips_end_app()

fips_begin_app(arraytex-wgpu windowed)
    fips_files(arraytex-wgpu.c)
    fips_deps(wgpu_entry)
fips_end_app()

fips_begin_app(dyntex-wgpu windowed)
    fips_files(dyntex-wgpu.c)
    fips_deps(wgpu_entry)
fips_end_app()

fips_begin_app(uvwrap-wgpu windowed)
    fips_files(uvwrap-wgpu.c)
    fips_deps(wgpu_entry)
fips_end_app()

fips_begin_app(mipmap-wgpu windowed)
    fips_files(mipmap-wgpu.c)
    fips_deps(wgpu_entry)
fips_end_app()

fips_begin_app(blend-wgpu windowed)
    fips_files(blend-wgpu.c)
    fips_deps(wgpu_entry)
fips_end_app()

fips_begin_app(imgui-wgpu windowed)
    fips_files(imgui-wgpu.cc)
    fips_deps(wgpu_entry imgui)
fips_end_app()

fips_begin_app(inject-wgpu windowed)
    fips_files(inject-wgpu.c)
    fips_deps(wgpu_entry)
fips_end_app()

fips_begin_app(drawcallperf-wgpu windowed)
    fips_files(drawcallperf-wgpu.c)
    fips_deps(wgpu_entry imgui)
fips_end_app()

fips_begin_app(vertexpulling-wgpu windowed)
    fips_files(vertexpulling-wgpu.c)
    fips_deps(wgpu_entry)
fips_end_app()

fips_begin_app(instancing-compute-wgpu windowed)
    fips_files(instancing-compute-wgpu.c)
    fips_deps(wgpu_entry)
fips_end_app()
