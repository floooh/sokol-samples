import type { Builder } from 'jsr:@floooh/fibs@^1';

export function addGlfwSamples(b: Builder) {
    addLibs(b);
    const stdSamples = [
        'clear',
        'triangle',
        'quad',
        'cube',
        'texcube',
        'instancing',
        'offscreen',
        'mrt',
        'arraytex',
        'dyntex',
        'mipmap',
        'blend',
        'inject',
        'bufferoffsets',
        'noninterleaved',
        'uniformarrays',
    ]
    stdSamples.forEach((name) => addSample(b, { name }));
    addSample(b, { name: 'imgui', ext: 'cc', deps: ['imgui'] });
    addSample(b, { name: 'multiwindow', deps: ['flextgl33']});
    addSample(b, { name: 'sgl-test', deps: ['flextgl12']});
    if (!b.isMacOS()) {
        addSample(b, { name: 'instancing-compute' });
        addSample(b, { name: 'vertexpulling' });
    }

    // special glfw+metal sample
    if (b.isMacOS()) {
        b.addTarget('metal-glfw', 'plain-exe', (t) => {
            t.setDir('glfw');
            t.addSource('metal-glfw.m');
            t.addDependencies(['glfw_glue']);
            t.addFrameworks(['Cocoa', 'QuartzCore', 'Metal']);
        });
    }
}

type SampleOptions = {
    name: string,
    ext?: 'cc',
    deps?: string[],
}

function addSample(b: Builder, opts: SampleOptions) {
    const { name, ext, deps } = opts;
    b.addTarget(`${name}-glfw`, 'plain-exe', (t) => {
        t.setDir('glfw');
        t.addSource(`${name}-glfw.${ext ?? 'c'}`);
        t.addDependencies(['glfw_glue']);
        if (deps) {
            t.addDependencies(deps);
        }
    });
}

function addLibs(b: Builder) {
    b.addTarget('flextgl12', 'lib', (t) => {
        t.setDir('glfw/flextgl12');
        t.addSources(['flextGL.c', 'flextGL.h']);
        if (b.isClang()) {
            t.addCompileOptions({ scope: 'private', opts: ['-Wno-incompatible-function-pointer-types'] });
        }
        if (b.isClang() || b.isGcc()) {
            t.addCompileOptions({ scope: 'private', opts: ['-Wno-sign-conversion'] });
        }
    });
    b.addTarget('flextgl33', 'lib', (t) => {
        t.setDir('glfw/flextgl33');
        t.addSources(['flextGL.c', 'flextGL.h']);
        if (b.isClang()) {
            t.addCompileOptions({ scope: 'private', opts: ['-Wno-incompatible-function-pointer-types'] });
        }
        if (b.isClang() || b.isGcc()) {
            t.addCompileOptions({ scope: 'private', opts: ['-Wno-sign-conversion', '-Wno-missing-braces', '-Wno-unused-parameter'] });
        }
    });
    b.addTarget('glfw_glue', 'lib', (t) => {
        const sokolDir = `${b.importDir('sokol')}`;
        t.setDir('glfw');
        t.addSources(['glfw_glue.c', 'glfw_glue.h']);
        t.addDependencies(['glfw3']);
        t.addIncludeDirectories([sokolDir, `${sokolDir}/util`]);
    });
}
