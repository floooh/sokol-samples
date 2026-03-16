import type { Builder } from 'jsr:@floooh/fibs@^1';

export function addMetalSamples(b: Builder) {
    if (!(b.isMacOS() || b.isIOS())) {
        throw new Error('Metal samples can only be built for macOS or iOS');
    }
    addLibs(b);
    const stdSamples = [
        'clear',
        'triangle',
        'quad',
        'cube',
        'texcube',
        'offscreen',
        'mrt',
        'arraytex',
        'dyntex',
        'mipmap',
        'blend',
        'releasetest',
        'bufferoffsets',
        'noninterleaved',
        'vertexpulling',
        'instancing-compute',
    ]
    stdSamples.forEach((name) => addSample(b, { name }));
    addSample(b, { name: 'inject', ext: 'm' });
    addSample(b, { name: 'imgui', ext: 'cc', deps: ['imgui'] });
    b.addTarget('binshader-metal', 'windowed-exe', (t) => {
        t.setDir('metal');
        t.addSource('binshader-metal.c');
        if (b.isMacOS()) {
            t.addSource('binshader-metal-macosx.h');
        } else {
            t.addSource('binshader-metal-iphoneos.h');
        }
        t.addDependencies(['osxentry']);
    });
}

type SampleOptions = {
    name: string,
    ext?: 'cc' | 'm',
    deps?: string[],
}

function addSample(b: Builder, opts: SampleOptions) {
    const { name, ext, deps } = opts;
    b.addTarget(`${name}-metal`, 'windowed-exe', (t) => {
        t.setDir('metal');
        t.addSource(`${name}-metal.${ext ?? 'c'}`);
        t.addDependencies(['osxentry']);
        if (deps) {
            t.addDependencies(deps);
        }
    });
}

function addLibs(b: Builder) {
    b.addTarget('osxentry', 'lib', (t) => {
        const sokolDir = b.importDir('sokol');
        t.setDir('metal');
        t.addSources(['osxentry.m', 'osxentry.h', 'sokol_gfx.m']);
        t.addIncludeDirectories([sokolDir, `${sokolDir}/util`]);
        t.addFrameworks(['Foundation']);
        if (b.isMacOS()) {
            t.addFrameworks(['Cocoa', 'Metal', 'MetalKit', 'QuartzCore']);
        } else if (b.isIOS()) {
            t.addFrameworks(['UIKit', 'Metal', 'MetalKit']);
        }
    });
}
