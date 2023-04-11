/*
    Experimental build description for https://github.com/floooh/fibs

    (please ignore for now)
*/
import * as fibs from 'https://raw.githubusercontent.com/floooh/fibs/main/mod.ts'

type Sample = {
    name: string,
    ext: 'c' | 'cc' | 'm' | 'mm',
    libs: string[],
    type: ('metal' | 'glfw' | 'emsc')[],
};

const samples: Sample[] = [
    { name: 'arraytex',         ext: 'c',  libs: [], type: ['metal','glfw','emsc'] },
    { name: 'binshader',        ext: 'c',  libs: [], type: ['metal'] },
    { name: 'blend',            ext: 'c',  libs: [], type: ['metal','glfw','emsc'] },
    { name: 'bufferoffsets',    ext: 'c',  libs: [], type: ['metal','glfw','emsc'] },
    { name: 'clear',            ext: 'c',  libs: [], type: ['metal','glfw','emsc'] },
    { name: 'cube',             ext: 'c',  libs: [], type: ['metal','glfw','emsc'] },
    { name: 'dyntex',           ext: 'c',  libs: [], type: ['metal','glfw','emsc'] },
    { name: 'imgui',            ext: 'cc', libs: ['imgui'], type: ['metal','glfw','emsc'] },
    { name: 'inject',           ext: 'c', libs: [], type: ['metal','glfw','emsc'] },
    { name: 'instancing',       ext: 'c',  libs: [], type: ['metal','glfw','emsc'] },
    { name: 'mipmap',           ext: 'c',  libs: [], type: ['metal','glfw','emsc'] },
    { name: 'mrt',              ext: 'c',  libs: [], type: ['metal','glfw','emsc'] },
    { name: 'multiwindow',      ext: 'c',  libs: [], type: ['glfw'] },
    { name: 'noninterleaved',   ext: 'c',  libs: [], type: ['metal','glfw','emsc'] },
    { name: 'offscreen',        ext: 'c',  libs: [], type: ['metal','glfw','emsc'] },
    { name: 'quad',             ext: 'c',  libs: [], type: ['metal','glfw','emsc'] },
    { name: 'releasetest',      ext: 'c',  libs: [], type: ['metal'] },
    { name: 'texcube',          ext: 'c',  libs: [], type: ['metal','glfw','emsc'] },
    { name: 'triangle',         ext: 'c',  libs: [], type: ['metal','glfw','emsc'] },
    { name: 'uniformarrays',    ext: 'c',  libs: [], type: ['glfw'] },
];

// ### METAL SAMPLES ###
export const metal_targets= () => {
    const enabled = (ctx: fibs.Context) => ctx.config.name.startsWith('metal-');
    const targets: Record<string, fibs.TargetDesc> = {
        'entry_metal': {
            enabled,
            type: 'lib',
            dir: 'metal',
            sources: () => [ 'osxentry.m', 'osxentry.h', 'sokol.m' ],
            libs: (context) => {
                const libs = [ 'sokol-includes', '-framework Metal', '-framework MetalKit' ];
                if (context.config.platform === 'macos') {
                    libs.push('-framework Cocoa', '-framework QuartzCore');
                } else {
                    libs.push('-framework UIKit');
                }
                return libs;
            },
        },
    };
    samples.forEach((sample) => {
        if (sample.type.includes('metal')) {
            const ext = sample.name === 'inject' ? 'mm' : sample.ext;
            targets[`${sample.name}-metal`] = {
                enabled,
                type: 'windowed-exe',
                dir: 'metal',
                sources: () => [ `${sample.name}-metal.${ext}` ],
                libs: () => [ 'entry_metal', ...sample.libs ],
            }
        }
    });
    return targets;
}

// ### EMSCRIPTEN SAMPLES ###
export const emscripten_targets = () => {
    const targets: Record<string, fibs.TargetDesc> = {};
    samples.forEach((sample) => {
        if (sample.type.includes('emsc')) {
            targets[`${sample.name}-emsc`] = {
                enabled: (ctx) => ctx.config.name.startsWith('emsc-'),
                type: 'windowed-exe',
                dir: 'html5',
                sources: () => [ `${sample.name}-emsc.${sample.ext}` ],
                libs: () => [ 'sokol-includes', ...sample.libs ],
                linkOptions: { public: () => [ '-sUSE_WEBGL2=1', "-sMALLOC='emmalloc'" ] },
            }
        }
    });
    return targets;
}

// ### GLFW SAMPLES ###
export const glfw_targets = () => {
    const targets: Record<string, fibs.TargetDesc> = {};
    samples.forEach((sample) => {
        if (sample.type.includes('glfw')) {
            targets[`${sample.name}-glfw`] = {
                enabled: (ctx) => ctx.config.name.startsWith('glfw-'),
                type: 'plain-exe',
                dir: 'glfw',
                sources: () => [ `${sample.name}-glfw.${sample.ext}` ],
                libs: () => [ 'sokol-includes', 'glfw3', ...sample.libs ],
            }
        }
    });
    // special metal-glfw target
    targets['metal-glfw'] = {
        enabled: (ctx) => ctx.config.name.startsWith('glfw-') && ctx.config.platform === 'macos',
        type: 'plain-exe',
        dir: 'glfw',
        sources: () => [ 'metal-glfw.m' ],
        libs: () => [ 'sokol-includes', 'glfw3', '-framework Metal', '-framework QuartzCore' ]
    };
    // special sgl-test-glfw target
    targets['sgl-test-glfw'] = {
        enabled: (ctx) => ctx.config.name.startsWith('glfw-'),
        type: 'plain-exe',
        dir: 'glfw',
        sources: () => [ 'sgl-test-glfw.c', 'flextgl12/flextGL.c' ],
        // suppress flextGL warnings on MSVC
        compileOptions: {
            private: (ctx) => (ctx.compiler === 'msvc') ? ['/wd4996', '/wd4152'] : []
        },
        libs: () => [ 'glfw3' ],
    };
    return targets;
}

export const project: fibs.ProjectDesc = {
    name: 'sokol-samples',
    imports: {
        libs: {
            url: 'https://github.com/floooh/fibs-libs',
            import: [ 'sokol.ts', 'imgui.ts', 'glfw3.ts' ],
        },
        utils: {
            url: 'https://github.com/floooh/fibs-utils',
            import: [ 'stdoptions.ts' ],
        }
    },
    targets: {
        // only enable glfw3 target for glfw build configs
        glfw3: {
            enabled: (context) => context.config.name.startsWith('glfw-'),
        },
        ...metal_targets(),
        ...emscripten_targets(),
        ...glfw_targets(),
    },
    configs: {
        // use these configs to build the samples under metal/
        'metal-macos-ninja-debug': { inherits: 'macos-ninja-debug' },
        'metal-macos-ninja-release': { inherits: 'macos-ninja-release' },
        'metal-macos-make-debug': { inherits: 'macos-make-debug' },
        'metal-macos-make-release': { inherits: 'macos-make-release' },
        'metal-macos-vscode-debug': { inherits: 'macos-vscode-debug' },
        'metal-macos-vscode-release': { inherits: 'macos-vscode-release' },
        'metal-macos-xcode-debug': { inherits: 'macos-xcode-debug' },
        'metal-macos-xcode-release': { inherits: 'macos-xcode-release' },
        // use these configs to build the samples under glfw/
        'glfw-macos-ninja-debug': { inherits: 'macos-ninja-debug' },
        'glfw-macos-ninja-release': { inherits: 'macos-ninja-release' },
        'glfw-macos-make-debug': { inherits: 'macos-make-debug' },
        'glfw-macos-make-release': { inherits: 'macos-make-release' },
        'glfw-macos-vscode-debug': { inherits: 'macos-vscode-debug' },
        'glfw-macos-vscode-release': { inherits: 'macos-vscode-release' },
        'glfw-macos-xcode-debug': { inherits: 'macos-xcode-debug' },
        'glfw-macos-xcode-release': { inherits: 'macos-xcode-release' },
        'glfw-win-vstudio-debug': { inherits: 'win-vstudio-debug' },
        'glfw-win-vstudio-release': { inherits: 'win-vstudio-release' },
    }
}
