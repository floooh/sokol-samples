import type { Builder } from 'jsr:@floooh/fibs@^1';

export function addD3d11Samples(b: Builder) {
    b.addTarget('d3d11entry', 'lib', (t) => {
        const sokolDir = b.importDir('sokol');
        t.setDir('d3d11');
        t.addSources(['d3d11entry.c', 'd3d11entry.h']);
        t.addIncludeDirectories([sokolDir, `${sokolDir}/util`]);
    });
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
        'binshader',
        'blend',
        'inject',
        'bufferoffsets',
        'vertexpulling',
    ];
    stdSamples.forEach((name) => addSample(b, { name }));
    addSample(b, { name: 'imgui', ext: 'cc', deps: ['imgui']});
}

type SampleOptions = {
    name: string,
    ext?: 'cc',
    deps?: string[],
}

function addSample(b: Builder, opts: SampleOptions) {
    const { name, ext, deps } = opts;
    b.addTarget(`${name}-d3d11`, 'windowed-exe', (t) => {
        t.setDir('d3d11');
        t.addSource(`${name}-d3d11.${ext ?? 'c'}`);
        t.addDependencies(['d3d11entry']);
        if (deps) {
            t.addDependencies(deps);
        }
    });
}