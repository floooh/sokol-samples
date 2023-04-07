/*
    Experimental build description for https://github.com/floooh/fibs

    (please ignore for now)
*/
import * as fibs from 'https://raw.githubusercontent.com/floooh/fibs/main/mod.ts'

type Sample = {
    name: string,
    type: 'c' | 'cc' | 'mm',
    libs: string[],
};

// ### METAL SAMPLES ###
export const metal_targets= () => {
    const enabled = (context: fibs.ProjectBuildContext) => context.config.name.startsWith('metal-');
    const samples: Sample[] = [
        { name: 'arraytex',         type: 'c',  libs: [] },
        { name: 'binshader',        type: 'c',  libs: [] },
        { name: 'blend',            type: 'c',  libs: [] },
        { name: 'bufferoffsets',    type: 'c',  libs: [] },
        { name: 'clear',            type: 'c',  libs: [] },
        { name: 'cube',             type: 'c',  libs: [] },
        { name: 'dyntex',           type: 'c',  libs: [] },
        { name: 'instancing',       type: 'c',  libs: [] },
        { name: 'mipmap',           type: 'c',  libs: [] },
        { name: 'mrt',              type: 'c',  libs: [] },
        { name: 'noninterleaved',   type: 'c',  libs: [] },
        { name: 'offscreen',        type: 'c',  libs: [] },
        { name: 'quad',             type: 'c',  libs: [] },
        { name: 'releasetest',      type: 'c',  libs: [] },
        { name: 'texcube',          type: 'c',  libs: [] },
        { name: 'triangle',         type: 'c',  libs: [] },
        { name: 'imgui',            type: 'cc', libs: [ 'imgui' ] },
        { name: 'inject',           type: 'mm', libs: [] },
    ];

    const targets: Record<string, fibs.TargetDesc> = {
        'entry_metal': {
            enabled,
            type: 'lib',
            dir: 'metal',
            sources: [ 'osxentry.m', 'osxentry.h', 'sokol.m' ],
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
        targets[`${sample.name}-metal`] = {
            enabled,
            type: 'windowed-exe',
            dir: 'metal',
            sources: [ `${sample.name}-metal.${sample.type}` ],
            libs: [ 'entry_metal', ...sample.libs ],
        }
    });
    return targets;
}

export const project: fibs.ProjectDesc = {
    name: 'sokol-samples',
    imports: {
        libs: {
            url: 'https://github.com/floooh/fibs-libs',
            import: [ 'sokol.ts', 'imgui.ts' ],
        },
        utils: {
            url: 'https://github.com/floooh/fibs-utils',
            import: [ 'stdoptions.ts' ],
        }
    },
    targets: {
        ...metal_targets(),
    },
    configs: {
        // use these configs to build the 'raw' Metal samples
        'metal-macos-ninja-debug': { inherits: 'macos-ninja-debug' },
        'metal-macos-ninja-release': { inherits: 'macos-ninja-release' },
        'metal-macos-make-debug': { inherits: 'macos-make-debug' },
        'metal-macos-make-release': { inherits: 'macos-make-release' },
        'metal-macos-vscode-debug': { inherits: 'macos-vscode-debug' },
        'metal-macos-vscode-release': { inherits: 'macos-vscode-release' },
        'metal-macos-xcode-debug': { inherits: 'macos-xcode-debug' },
        'metal-macos-xcode-release': { inherits: 'macos-xcode-release' },
    }
}
