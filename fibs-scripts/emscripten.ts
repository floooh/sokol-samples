import type { Builder } from 'jsr:@floooh/fibs@^1';

export function addEmscriptenSamples(b: Builder) {
    const stdSamples = [
        'clear',
        'triangle',
        'quad',
        'cube',
        'texcube',
        'instancing',
        'offscreen',
        'dyntex',
        'blend',
        'inject',
        'bufferoffsets',
        'noninterleaved',
        'mrt',
        'arraytex',
        'mipmap'
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
    b.addTarget(`${name}-emsc`, 'windowed-exe', (t) => {
        const sokolDir = b.importDir('sokol');
        t.setDir('html5');
        t.addSource(`${name}-emsc.${ext ?? 'c'}`);
        t.addIncludeDirectories([sokolDir, `${sokolDir}/util`]);
        if (deps) {
            t.addDependencies(deps);
        }
        t.addLinkOptions(['-sUSE_WEBGL2=1']);
    });
}
