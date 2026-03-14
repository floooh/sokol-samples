import { type Builder, conf, type Config, type ConfigDesc, type Configurer, type Project, type TargetBuilder, type TargetJob, util } from 'jsr:@floooh/fibs@^1';

const ANDROID_ABI = 'armeabi-v7a';
const ANDROID_PLATFORM = 'android-30';

const sokolAppSamples: SokolAppSampleOptions[] = [
    { name: 'clear', ui: 'cc' },
    { name: 'triangle', ui: 'cc', shd: true },
    { name: 'triangle-bufferless', ui: 'cc', shd: true },
    { name: 'quad', ui: 'cc', shd: true },
    { name: 'bufferoffsets', ui: 'cc', shd: true },
    { name: 'drawex', ui: 'cc', shd: true },
    { name: 'cube', ui: 'cc', shd: true },
    { name: 'noninterleaved', ui: 'cc', shd: true },
    { name: 'vertexpull', ui: 'cc', shd: true },
    { name: 'vertexindexbuffer', ui: 'cc', shd: true },
    { name: 'texcube', ui: 'cc', shd: true },
    { name: 'sbuftex', ui: 'cc', shd: true },
    { name: 'sbufoffset', ui: 'cc', shd: true },
    { name: 'vertextexture', ui: 'cc', shd: true },
    { name: 'offscreen', ui: 'cc', shd: true },
    { name: 'offscreen-msaa', ui: 'cc', shd: true },
    { name: 'shadows', ui: 'cc', shd: true },
    { name: 'shadows-depthtex', ui: 'cc', shd: true },
    { name: 'instancing', ui: 'cc', shd: true },
    { name: 'instancing-pull', ui: 'cc', shd: true },
    { name: 'mrt', ui: 'cc', shd: true },
    { name: 'mrt-pixelformats', ui: 'cc', shd: true },
    { name: 'arraytex', ui: 'cc', shd: true },
    { name: 'tex3d', ui: 'cc', shd: true },
    { name: 'dyntex3d', shd: true, deps: ['imgui'] },
    { name: 'dyntex', ui: 'cc', shd: true },
    { name: 'shared-bindings', ui: 'cc', shd: true },
    { name: 'customresolve', shd: true, deps: ['imgui'] },
    { name: 'mipmap', ui: 'cc', shd: true },
    {
        name: 'cubemap-jpeg',
        ui: 'cc',
        shd: true,
        deps: ['stb-lib', 'fileutil'],
        jobs: copy('data/nissibeach2', [
            'nb2_negx.jpg',
            'nb2_negy.jpg',
            'nb2_negz.jpg',
            'nb2_posx.jpg',
            'nb2_posy.jpg',
            'nb2_posz.jpg',
        ]),
    },
];

export function configure(c: Configurer) {
    addConfigs(c);
    c.addImport({
        name: 'extras',
        url: 'https://github.com/floooh/fibs-extras',
        files: [
            'stdoptions.ts',
            'macos.ts',
            'windows.ts',
            'ios.ts',
            'android.ts',
            'emscripten.ts',
            'linux-threads.ts',
            'copyfiles.ts',
            'embedfiles.ts',
            'sokolshdc.ts',
            'vscode.ts',
        ],
    });
    c.addImport({
        name: 'libs',
        url: 'https://github.com/floooh/fibs-libs',
        // FIXME: nuklear, microui, stb, glfw3
        files: ['sokol.ts', 'stb.ts'],
    });
    c.addImport({
        name: 'dcimgui',
        url: 'https://github.com/floooh/dcimgui',
        files: ['fibs.ts', 'fibs-docking.ts'],
    });
    c.addImportOptions((p: Project) => {
        return {
            sokol: {
                backend: sokolBackendByConfig(p.activeConfig()),
                useEGL: p.activeConfig().name.includes('-egl-'),
            },
            // FIXME: Android import options
        };
    });
}

export function build(b: Builder) {
    addLibs(b);
    sokolAppSamples.forEach((s) => addSokolAppSample(b, s));
}

type SokolAppSampleOptions = {
    name: string;
    ext?: 'cc';
    ui?: 'c' | 'cc';
    sokol?: 'cc' | 'dll' | 'noentry';
    shd?: true;
    deps?: string[];
    jobs?: TargetJob[];
};

function addSokolAppSample(b: Builder, { name, ext, ui, sokol, shd, deps, jobs }: SokolAppSampleOptions) {
    const cmn = (t: TargetBuilder) => {
        t.setDir('sapp');
        t.addSource(`${name}-sapp.${ext ?? 'c'}`);
        t.addIncludeDirectories(['../libs']);
        if (sokol === 'cc') {
            t.addDependencies(['sokol-cpp-lib']);
        } else if (sokol === 'noentry') {
            t.addDependencies(['sokol-noentry']);
        } else if (sokol === 'dll') {
            t.addDependencies(['sokol-dll']);
        } else {
            t.addDependencies(['sokol-lib']);
        }
        if (deps) {
            t.addDependencies(deps);
        }
        if (shd) {
            t.addIncludeDirectories([t.buildDir()]);
            t.addJob({
                job: 'sokolshdc',
                args: { src: `${name}-sapp.glsl`, outDir: t.buildDir() },
            });
        }
        if (jobs) {
            jobs.forEach((j) => t.addJob(j));
        }
    };
    b.addTarget(`${name}-sapp`, 'windowed-exe', (t) => cmn(t));
    if (ui) {
        b.addTarget(`${name}-sapp-ui`, 'windowed-exe', (t) => {
            cmn(t);
            t.addCompileDefinitions({ USE_DBG_UI: '1' });
            if (ui === 'c') {
                t.addDependencies(['cdbgui']);
            } else if (ui === 'cc') {
                t.addDependencies(['dbgui']);
            }
        });
    }
}

function addLibs(b: Builder) {
    // the various sokol-library flavours
    b.addTarget('sokol-lib', 'lib', (t) => {
        t.setDir('libs/sokol');
        t.addSource('sokol.c');
        t.addDependencies(['sokol']);
    });
    b.addTarget('sokol-cpp-lib', 'lib', (t) => {
        t.setDir('libs/sokol');
        t.addSource('sokol.cc');
        t.addDependencies(['sokol']);
    });
    if (!b.isAndroid()) {
        b.addTarget('sokol-noentry', 'lib', (t) => {
            t.setDir('libs/sokol');
            t.addSource('sokol-noentry.c');
            t.addDependencies(['sokol']);
        });
    }
    if (b.isWindows() || b.isMacOS() || b.isLinux()) {
        b.addTarget('sokol-dll', 'dll', (t) => {
            t.setDir('libs/sokol');
            t.addSource('sokol-dll.c');
            t.addDependencies(['sokol']);
        });
    }

    // debug ui in cpp and c flavours
    b.addTarget('dbgui', 'lib', (t) => {
        t.setDir('libs/dbgui');
        t.addSources(['dbgui.cc', 'dbgui.h']);
        t.addDependencies(['sokol', 'imgui']);
    });
    b.addTarget('cdbgui', 'lib', (t) => {
        t.setDir('libs/cdbgui');
        t.addSources(['cdbgui.c', 'cdbgui.h']);
        t.addDependencies(['sokol', 'imgui']);
    });

    // misc libs
    b.addTarget('stb-lib', 'lib', (t) => {
        t.setDir('libs/stb');
        t.addSource('stb_image.c');
        t.addDependencies(['stb']);
    });
    b.addTarget('fileutil', 'lib', (t) => {
        t.setDir('libs/util');
        t.addSource('fileutil.h');
        if (b.isMacOS() || b.isIOS()) {
            t.addSource('fileutil_osx.m');
        } else {
            t.addSource('fileutil.c');
        }
    });
}

function sokolBackendByConfig(config: Config) {
    const n = config.name;
    if (n.includes('-gl-')) return 'glcore';
    if (n.includes('-gles-')) return 'gles3';
    if (n.includes('-d3d11-')) return 'd3d11';
    if (n.includes('-metal-')) return 'metal';
    if (n.includes('-wgpu-')) return 'wgpu';
    if (n.includes('-vk-')) return 'vulkan';
    // otherwise: autoselect
    return undefined;
}

function copy(srcDir: string, files: string[]): TargetJob[] {
    return [{
        job: 'copyfiles',
        args: { srcDir, files },
    }];
}

function addConfigs(c: Configurer) {
    const cfg = conf.builtinConfigDesc;

    // windows + d3d11
    c.addConfig({ ...cfg('win-msvc-debug'), name: 'sapp-d3d11-win-msvc-debug' });
    c.addConfig({ ...cfg('win-msvc-release'), name: 'sapp-d3d11-win-msvc-release' });
    c.addConfig({ ...cfg('win-msvc-debug'), name: 'sapp-d3d11-win-vstudio-debug', opener: 'vstudio' });
    c.addConfig({ ...cfg('win-msvc-release'), name: 'sapp-d3d11-win-vstudio-release', opener: 'vstudio' });
    c.addConfig({ ...cfg('win-msvc-debug'), name: 'sapp-d3d11-win-vscode-debug', generator: 'ninja', opener: 'vscode' });
    c.addConfig({ ...cfg('win-msvc-release'), name: 'sapp-d3d11-win-vscode-release', generator: 'ninja', opener: 'vscode' });

    // windows + gl
    c.addConfig({ ...cfg('win-msvc-debug'), name: 'sapp-gl-win-msvc-debug' });
    c.addConfig({ ...cfg('win-msvc-release'), name: 'sapp-gl-win-msvc-release' });
    c.addConfig({ ...cfg('win-msvc-debug'), name: 'sapp-gl-win-vstudio-debug', opener: 'vstudio' });
    c.addConfig({ ...cfg('win-msvc-release'), name: 'sapp-gl-win-vstudio-release', opener: 'vstudio' });
    c.addConfig({ ...cfg('win-msvc-debug'), name: 'sapp-gl-win-vscode-debug', generator: 'ninja', opener: 'vscode' });
    c.addConfig({ ...cfg('win-msvc-release'), name: 'sapp-gl-win-vscode-release', generator: 'ninja', opener: 'vscode' });

    // windows + vulkan
    c.addConfig({ ...cfg('win-msvc-debug'), name: 'sapp-vk-win-msvc-debug' });
    c.addConfig({ ...cfg('win-msvc-release'), name: 'sapp-vk-win-msvc-release' });
    c.addConfig({ ...cfg('win-msvc-debug'), name: 'sapp-vk-win-vstudio-debug', opener: 'vstudio' });
    c.addConfig({ ...cfg('win-msvc-release'), name: 'sapp-vk-win-vstudio-release', opener: 'vstudio' });
    c.addConfig({ ...cfg('win-msvc-debug'), name: 'sapp-vk-win-vscode-debug', generator: 'ninja', opener: 'vscode' });
    c.addConfig({ ...cfg('win-msvc-release'), name: 'sapp-vk-win-vscode-release', generator: 'ninja', opener: 'vscode' });

    // windows + webgpu
    c.addConfig({ ...cfg('win-msvc-debug'), name: 'sapp-wgpu-win-msvc-debug' });
    c.addConfig({ ...cfg('win-msvc-release'), name: 'sapp-wgpu-win-msvc-release' });
    c.addConfig({ ...cfg('win-msvc-debug'), name: 'sapp-wgpu-win-vstudio-debug', opener: 'vstudio' });
    c.addConfig({ ...cfg('win-msvc-release'), name: 'sapp-wgpu-win-vstudio-release', opener: 'vstudio' });
    c.addConfig({ ...cfg('win-msvc-debug'), name: 'sapp-wgpu-win-vscode-debug', generator: 'ninja', opener: 'vscode' });
    c.addConfig({ ...cfg('win-msvc-release'), name: 'sapp-wgpu-win-vscode-release', generator: 'ninja', opener: 'vscode' });

    // macos + metal
    c.addConfig({ ...cfg('macos-make-release'), name: 'sapp-metal-macos-make-release' });
    c.addConfig({ ...cfg('macos-make-debug'), name: 'sapp-metal-macos-make-debug' });
    c.addConfig({ ...cfg('macos-ninja-release'), name: 'sapp-metal-macos-ninja-release' });
    c.addConfig({ ...cfg('macos-ninja-debug'), name: 'sapp-metal-macos-ninja-debug' });
    c.addConfig({ ...cfg('macos-ninja-release'), name: 'sapp-metal-macos-vscode-release', opener: 'vscode' });
    c.addConfig({ ...cfg('macos-ninja-debug'), name: 'sapp-metal-macos-vscode-debug', opener: 'vscode' });
    c.addConfig({ ...cfg('macos-ninja-release'), name: 'sapp-metal-macos-xcode-release', generator: 'xcode', opener: 'xcode' });
    c.addConfig({ ...cfg('macos-ninja-debug'), name: 'sapp-metal-macos-xcode-debug', generator: 'xcode', opener: 'xcode' });

    // macos + gl
    c.addConfig({ ...cfg('macos-make-release'), name: 'sapp-gl-macos-make-release' });
    c.addConfig({ ...cfg('macos-make-debug'), name: 'sapp-gl-macos-make-debug' });
    c.addConfig({ ...cfg('macos-ninja-release'), name: 'sapp-gl-macos-ninja-release' });
    c.addConfig({ ...cfg('macos-ninja-debug'), name: 'sapp-gl-macos-ninja-debug' });
    c.addConfig({ ...cfg('macos-ninja-release'), name: 'sapp-gl-macos-vscode-release', opener: 'vscode' });
    c.addConfig({ ...cfg('macos-ninja-debug'), name: 'sapp-gl-macos-vscode-debug', opener: 'vscode' });
    c.addConfig({ ...cfg('macos-ninja-release'), name: 'sapp-gl-macos-xcode-release', generator: 'xcode', opener: 'xcode' });
    c.addConfig({ ...cfg('macos-ninja-debug'), name: 'sapp-gl-macos-xcode-debug', generator: 'xcode', opener: 'xcode' });

    // macos + wgpu
    c.addConfig({ ...cfg('macos-make-release'), name: 'sapp-wgpu-macos-make-release' });
    c.addConfig({ ...cfg('macos-make-debug'), name: 'sapp-wgpu-macos-make-debug' });
    c.addConfig({ ...cfg('macos-ninja-release'), name: 'sapp-wgpu-macos-ninja-release' });
    c.addConfig({ ...cfg('macos-ninja-debug'), name: 'sapp-wgpu-macos-ninja-debug' });
    c.addConfig({ ...cfg('macos-ninja-release'), name: 'sapp-wgpu-macos-vscode-release', opener: 'vscode' });
    c.addConfig({ ...cfg('macos-ninja-debug'), name: 'sapp-wgpu-macos-vscode-debug', opener: 'vscode' });
    c.addConfig({ ...cfg('macos-ninja-release'), name: 'sapp-wgpu-macos-xcode-release', generator: 'xcode', opener: 'xcode' });
    c.addConfig({ ...cfg('macos-ninja-debug'), name: 'sapp-wgpu-macos-xcode-debug', generator: 'xcode', opener: 'xcode' });

    // linux + gl
    c.addConfig({ ...cfg('linux-make-release'), name: 'sapp-gl-linux-make-release' });
    c.addConfig({ ...cfg('linux-make-debug'), name: 'sapp-gl-linux-make-debug' });
    c.addConfig({ ...cfg('linux-ninja-release'), name: 'sapp-gl-linux-ninja-release' });
    c.addConfig({ ...cfg('linux-ninja-debug'), name: 'sapp-gl-linux-ninja-debug' });
    c.addConfig({ ...cfg('linux-ninja-release'), name: 'sapp-gl-linux-vscode-release', opener: 'vscode' });
    c.addConfig({ ...cfg('linux-ninja-debug'), name: 'sapp-gl-linux-vscode-debug', opener: 'vscode' });

    // linux + gl + egl
    c.addConfig({ ...cfg('linux-make-release'), name: 'sapp-gl-egl-linux-make-release' });
    c.addConfig({ ...cfg('linux-make-debug'), name: 'sapp-gl-egl-linux-make-debug' });
    c.addConfig({ ...cfg('linux-ninja-release'), name: 'sapp-gl-egl-linux-ninja-release' });
    c.addConfig({ ...cfg('linux-ninja-debug'), name: 'sapp-gl-egl-linux-ninja-debug' });
    c.addConfig({ ...cfg('linux-ninja-release'), name: 'sapp-gl-egl-linux-vscode-release', opener: 'vscode' });
    c.addConfig({ ...cfg('linux-ninja-debug'), name: 'sapp-gl-egl-linux-vscode-debug', opener: 'vscode' });

    // linux + gles3 + egl
    c.addConfig({ ...cfg('linux-make-release'), name: 'sapp-gles-egl-linux-make-release' });
    c.addConfig({ ...cfg('linux-make-debug'), name: 'sapp-gles-egl-linux-make-debug' });
    c.addConfig({ ...cfg('linux-ninja-release'), name: 'sapp-gles-egl-linux-ninja-release' });
    c.addConfig({ ...cfg('linux-ninja-debug'), name: 'sapp-gles-egl-linux-ninja-debug' });
    c.addConfig({ ...cfg('linux-ninja-release'), name: 'sapp-gles-egl-linux-vscode-release', opener: 'vscode' });
    c.addConfig({ ...cfg('linux-ninja-debug'), name: 'sapp-gles-egl-linux-vscode-debug', opener: 'vscode' });

    // linux + vulkan
    c.addConfig({ ...cfg('linux-make-release'), name: 'sapp-vk-linux-make-release' });
    c.addConfig({ ...cfg('linux-make-debug'), name: 'sapp-vk-linux-make-debug' });
    c.addConfig({ ...cfg('linux-ninja-release'), name: 'sapp-vk-linux-ninja-release' });
    c.addConfig({ ...cfg('linux-ninja-debug'), name: 'sapp-vk-linux-ninja-debug' });
    c.addConfig({ ...cfg('linux-ninja-release'), name: 'sapp-vk-linux-vscode-release', opener: 'vscode' });
    c.addConfig({ ...cfg('linux-ninja-debug'), name: 'sapp-vk-linux-vscode-debug', opener: 'vscode' });

    // linux + webgpu
    c.addConfig({ ...cfg('linux-make-release'), name: 'sapp-wgpu-linux-make-release' });
    c.addConfig({ ...cfg('linux-make-debug'), name: 'sapp-wgpu-linux-make-debug' });
    c.addConfig({ ...cfg('linux-ninja-release'), name: 'sapp-wgpu-linux-ninja-release' });
    c.addConfig({ ...cfg('linux-ninja-debug'), name: 'sapp-wgpu-linux-ninja-debug' });
    c.addConfig({ ...cfg('linux-ninja-release'), name: 'sapp-wgpu-linux-vscode-release', opener: 'vscode' });
    c.addConfig({ ...cfg('linux-ninja-debug'), name: 'sapp-wgpu-linux-vscode-debug', opener: 'vscode' });

    // ios + metal
    c.addConfig({
        ...cfg('macos-ninja-release'),
        name: 'sapp-metal-ios-xcode-release',
        platform: 'ios',
        generator: 'xcode',
        opener: 'xcode',
        cmakeCacheVariables: { CMAKE_SYSTEM_NAME: 'iOS' },
    });
    c.addConfig({
        ...cfg('macos-ninja-debug'),
        name: 'sapp-metal-ios-xcode-debug',
        platform: 'ios',
        generator: 'xcode',
        opener: 'xcode',
        cmakeCacheVariables: { CMAKE_SYSTEM_NAME: 'iOS' },
    });

    // ios + gles
    c.addConfig({
        ...cfg('macos-ninja-release'),
        name: 'sapp-gles-ios-xcode-release',
        platform: 'ios',
        generator: 'xcode',
        opener: 'xcode',
        cmakeCacheVariables: { CMAKE_SYSTEM_NAME: 'iOS' },
    });
    c.addConfig({
        ...cfg('macos-ninja-debug'),
        name: 'sapp-gles-ios-xcode-debug',
        platform: 'ios',
        generator: 'xcode',
        opener: 'xcode',
        cmakeCacheVariables: { CMAKE_SYSTEM_NAME: 'iOS' },
    });

    // emscripten
    const emsc: ConfigDesc = {
        name: 'emsc',
        platform: 'emscripten',
        runner: 'emscripten',
        buildMode: 'debug',
        toolchainFile: `${c.sdkDir()}/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake`,
        cmakeVariables: {
            CMAKE_EXECUTABLE_SUFFIX: '.html',
        },
        validate: () => {
            if (!util.dirExists(`${c.sdkDir()}/emsdk`)) {
                return {
                    valid: false,
                    hints: [`Emscripten SDK not installed (run 'fibs emsdk install')`],
                };
            } else {
                return { valid: true, hints: [] };
            }
        },
    };
    // emscripten + webgl2
    c.addConfig({ ...emsc, name: 'sapp-gles-emsc-make-release', generator: 'make', buildMode: 'release' });
    c.addConfig({ ...emsc, name: 'sapp-gles-emsc-make-debug', generator: 'make', buildMode: 'debug' });
    c.addConfig({ ...emsc, name: 'sapp-gles-emsc-ninja-release', generator: 'ninja', buildMode: 'release' });
    c.addConfig({ ...emsc, name: 'sapp-gles-emsc-ninja-debug', generator: 'ninja', buildMode: 'debug' });
    c.addConfig({ ...emsc, name: 'sapp-gles-emsc-vscode-release', generator: 'ninja', buildMode: 'release', opener: 'vscode' });
    c.addConfig({ ...emsc, name: 'sapp-gles-emsc-vscode-debug', generator: 'ninja', buildMode: 'debug', opener: 'vscode' });

    // emscripten + webgpu
    c.addConfig({ ...emsc, name: 'sapp-wgpu-emsc-make-release', generator: 'make', buildMode: 'release' });
    c.addConfig({ ...emsc, name: 'sapp-wgpu-emsc-make-debug', generator: 'make', buildMode: 'debug' });
    c.addConfig({ ...emsc, name: 'sapp-wgpu-emsc-ninja-release', generator: 'ninja', buildMode: 'release' });
    c.addConfig({ ...emsc, name: 'sapp-wgpu-emsc-ninja-debug', generator: 'ninja', buildMode: 'debug' });
    c.addConfig({ ...emsc, name: 'sapp-wgpu-emsc-vscode-release', generator: 'ninja', buildMode: 'release', opener: 'vscode' });
    c.addConfig({ ...emsc, name: 'sapp-wgpu-emsc-vscode-debug', generator: 'ninja', buildMode: 'debug', opener: 'vscode' });

    // android
    const android: ConfigDesc = {
        name: 'android',
        platform: 'android',
        runner: 'android',
        buildMode: 'debug',
        toolchainFile: `${c.sdkDir()}/android/ndk-bundle/build/cmake/android.toolchain.cmake`,
        cmakeCacheVariables: { ANDROID_ABI, ANDROID_PLATFORM },
        validate: () => {
            if (!util.dirExists(`${c.sdkDir()}/android`)) {
                return {
                    valid: false,
                    hints: [`Android SDK not installed (run 'fibs android install)`],
                };
            } else {
                return { valid: true, hints: [] };
            }
        },
    };

    // android + gles
    c.addConfig({ ...android, name: 'sapp-gles-android-make-release', generator: 'make', buildMode: 'release' });
    c.addConfig({ ...android, name: 'sapp-gles-android-make-debug', generator: 'make', buildMode: 'debug' });
    c.addConfig({ ...android, name: 'sapp-gles-android-ninja-release', generator: 'ninja', buildMode: 'release' });
    c.addConfig({ ...android, name: 'sapp-gles-android-ninja-debug', generator: 'ninja', buildMode: 'debug' });
    c.addConfig({ ...android, name: 'sapp-gles-android-vscode-release', generator: 'ninja', buildMode: 'release', opener: 'vscode' });
    c.addConfig({ ...android, name: 'sapp-gles-android-vscode-debug', generator: 'ninja', buildMode: 'debug', opener: 'vscode' });
}
