import { type Configurer, type ConfigDesc, conf, util } from 'jsr:@floooh/fibs@^1';

const ANDROID_ABI = 'armeabi-v7a';
const ANDROID_PLATFORM = 'android-30';

export function addConfigs(c: Configurer) {
    addSappConfigs(c);
    addGlfwConfigs(c);
}

const cfg = conf.builtinConfigDesc;

function addSappConfigs(c: Configurer) {
    const d3d11 = { sapp: true, backend: 'd3d11' };
    const gl = { sapp: true, backend: 'glcore' };
    const vk = { sapp: true, backend: 'vulkan' };
    const wgpu = { sapp: true, backend: 'wgpu' };
    const metal = { sapp: true, backend: 'metal' };
    const gles = { sapp: true, backend: 'gles3' };

    // override default configs
    c.addConfig({ ...cfg('win-msvc-release'), options: d3d11 });
    c.addConfig({ ...cfg('macos-make-release'), options: metal });
    c.addConfig({ ...cfg('linux-make-release'), options: gl });

    // windows + d3d11
    c.addConfig({ ...cfg('win-msvc-debug'), name: 'sapp-d3d11-win-msvc-debug', options: d3d11 });
    c.addConfig({ ...cfg('win-msvc-release'), name: 'sapp-d3d11-win-msvc-release', options: d3d11 });
    c.addConfig({ ...cfg('win-msvc-debug'), name: 'sapp-d3d11-win-vstudio-debug', opener: 'vstudio', options: d3d11 });
    c.addConfig({ ...cfg('win-msvc-release'), name: 'sapp-d3d11-win-vstudio-release', opener: 'vstudio', options: d3d11  });
    c.addConfig({ ...cfg('win-msvc-debug'), name: 'sapp-d3d11-win-vscode-debug', generator: 'ninja', opener: 'vscode', options: d3d11 });
    c.addConfig({ ...cfg('win-msvc-release'), name: 'sapp-d3d11-win-vscode-release', generator: 'ninja', opener: 'vscode', options: d3d11 });

    // windows + gl
    c.addConfig({ ...cfg('win-msvc-debug'), name: 'sapp-gl-win-msvc-debug', options: gl });
    c.addConfig({ ...cfg('win-msvc-release'), name: 'sapp-gl-win-msvc-release', options: gl });
    c.addConfig({ ...cfg('win-msvc-debug'), name: 'sapp-gl-win-vstudio-debug', opener: 'vstudio', options: gl });
    c.addConfig({ ...cfg('win-msvc-release'), name: 'sapp-gl-win-vstudio-release', opener: 'vstudio', options: gl });
    c.addConfig({ ...cfg('win-msvc-debug'), name: 'sapp-gl-win-vscode-debug', generator: 'ninja', opener: 'vscode', options: gl });
    c.addConfig({ ...cfg('win-msvc-release'), name: 'sapp-gl-win-vscode-release', generator: 'ninja', opener: 'vscode', options: gl });

    // windows + vulkan
    c.addConfig({ ...cfg('win-msvc-debug'), name: 'sapp-vk-win-msvc-debug', options: vk });
    c.addConfig({ ...cfg('win-msvc-release'), name: 'sapp-vk-win-msvc-release', options: vk });
    c.addConfig({ ...cfg('win-msvc-debug'), name: 'sapp-vk-win-vstudio-debug', opener: 'vstudio', options: vk });
    c.addConfig({ ...cfg('win-msvc-release'), name: 'sapp-vk-win-vstudio-release', opener: 'vstudio', options: vk });
    c.addConfig({ ...cfg('win-msvc-debug'), name: 'sapp-vk-win-vscode-debug', generator: 'ninja', opener: 'vscode', options: vk });
    c.addConfig({ ...cfg('win-msvc-release'), name: 'sapp-vk-win-vscode-release', generator: 'ninja', opener: 'vscode', options: vk });

    // windows + webgpu
    c.addConfig({ ...cfg('win-msvc-debug'), name: 'sapp-wgpu-win-msvc-debug', options: wgpu });
    c.addConfig({ ...cfg('win-msvc-release'), name: 'sapp-wgpu-win-msvc-release', options: wgpu });
    c.addConfig({ ...cfg('win-msvc-debug'), name: 'sapp-wgpu-win-vstudio-debug', opener: 'vstudio', options: wgpu });
    c.addConfig({ ...cfg('win-msvc-release'), name: 'sapp-wgpu-win-vstudio-release', opener: 'vstudio', options: wgpu });
    c.addConfig({ ...cfg('win-msvc-debug'), name: 'sapp-wgpu-win-vscode-debug', generator: 'ninja', opener: 'vscode', options: wgpu });
    c.addConfig({ ...cfg('win-msvc-release'), name: 'sapp-wgpu-win-vscode-release', generator: 'ninja', opener: 'vscode', options: wgpu });

    // macos + metal
    c.addConfig({ ...cfg('macos-make-release'), name: 'sapp-metal-macos-make-release', options: metal });
    c.addConfig({ ...cfg('macos-make-debug'), name: 'sapp-metal-macos-make-debug', options: metal });
    c.addConfig({ ...cfg('macos-ninja-release'), name: 'sapp-metal-macos-ninja-release', options: metal });
    c.addConfig({ ...cfg('macos-ninja-debug'), name: 'sapp-metal-macos-ninja-debug', options: metal });
    c.addConfig({ ...cfg('macos-ninja-release'), name: 'sapp-metal-macos-vscode-release', opener: 'vscode', options: metal });
    c.addConfig({ ...cfg('macos-ninja-debug'), name: 'sapp-metal-macos-vscode-debug', opener: 'vscode', options: metal });
    c.addConfig({ ...cfg('macos-ninja-release'), name: 'sapp-metal-macos-xcode-release', generator: 'xcode', opener: 'xcode', options: metal });
    c.addConfig({ ...cfg('macos-ninja-debug'), name: 'sapp-metal-macos-xcode-debug', generator: 'xcode', opener: 'xcode', options: metal });

    // macos + gl
    c.addConfig({ ...cfg('macos-make-release'), name: 'sapp-gl-macos-make-release', options: gl });
    c.addConfig({ ...cfg('macos-make-debug'), name: 'sapp-gl-macos-make-debug', options: gl });
    c.addConfig({ ...cfg('macos-ninja-release'), name: 'sapp-gl-macos-ninja-release', options: gl });
    c.addConfig({ ...cfg('macos-ninja-debug'), name: 'sapp-gl-macos-ninja-debug', options: gl });
    c.addConfig({ ...cfg('macos-ninja-release'), name: 'sapp-gl-macos-vscode-release', opener: 'vscode', options: gl });
    c.addConfig({ ...cfg('macos-ninja-debug'), name: 'sapp-gl-macos-vscode-debug', opener: 'vscode', options: gl });
    c.addConfig({ ...cfg('macos-ninja-release'), name: 'sapp-gl-macos-xcode-release', generator: 'xcode', opener: 'xcode', options: gl });
    c.addConfig({ ...cfg('macos-ninja-debug'), name: 'sapp-gl-macos-xcode-debug', generator: 'xcode', opener: 'xcode', options: gl });

    // macos + wgpu
    c.addConfig({ ...cfg('macos-make-release'), name: 'sapp-wgpu-macos-make-release', options: wgpu });
    c.addConfig({ ...cfg('macos-make-debug'), name: 'sapp-wgpu-macos-make-debug', options: wgpu });
    c.addConfig({ ...cfg('macos-ninja-release'), name: 'sapp-wgpu-macos-ninja-release', options: wgpu });
    c.addConfig({ ...cfg('macos-ninja-debug'), name: 'sapp-wgpu-macos-ninja-debug', options: wgpu });
    c.addConfig({ ...cfg('macos-ninja-release'), name: 'sapp-wgpu-macos-vscode-release', opener: 'vscode', options: wgpu });
    c.addConfig({ ...cfg('macos-ninja-debug'), name: 'sapp-wgpu-macos-vscode-debug', opener: 'vscode', options: wgpu });
    c.addConfig({ ...cfg('macos-ninja-release'), name: 'sapp-wgpu-macos-xcode-release', generator: 'xcode', opener: 'xcode', options: wgpu });
    c.addConfig({ ...cfg('macos-ninja-debug'), name: 'sapp-wgpu-macos-xcode-debug', generator: 'xcode', opener: 'xcode', options: wgpu });

    // linux + gl
    c.addConfig({ ...cfg('linux-make-release'), name: 'sapp-gl-linux-make-release', options: gl });
    c.addConfig({ ...cfg('linux-make-debug'), name: 'sapp-gl-linux-make-debug', options: gl });
    c.addConfig({ ...cfg('linux-ninja-release'), name: 'sapp-gl-linux-ninja-release', options: gl });
    c.addConfig({ ...cfg('linux-ninja-debug'), name: 'sapp-gl-linux-ninja-debug', options: gl });
    c.addConfig({ ...cfg('linux-ninja-release'), name: 'sapp-gl-linux-vscode-release', opener: 'vscode', options: gl });
    c.addConfig({ ...cfg('linux-ninja-debug'), name: 'sapp-gl-linux-vscode-debug', opener: 'vscode', options: gl });

    // linux + gl + egl
    c.addConfig({ ...cfg('linux-make-release'), name: 'sapp-gl-egl-linux-make-release', options: { ...gl, egl: true } });
    c.addConfig({ ...cfg('linux-make-debug'), name: 'sapp-gl-egl-linux-make-debug', options: { ...gl, egl: true } });
    c.addConfig({ ...cfg('linux-ninja-release'), name: 'sapp-gl-egl-linux-ninja-release', options: { ...gl, egl: true } });
    c.addConfig({ ...cfg('linux-ninja-debug'), name: 'sapp-gl-egl-linux-ninja-debug', options: { ...gl, egl: true } });
    c.addConfig({ ...cfg('linux-ninja-release'), name: 'sapp-gl-egl-linux-vscode-release', opener: 'vscode', options: { ...gl, egl: true } });
    c.addConfig({ ...cfg('linux-ninja-debug'), name: 'sapp-gl-egl-linux-vscode-debug', opener: 'vscode', options: { ...gl, egl: true } });

    // linux + gles3 + egl
    c.addConfig({ ...cfg('linux-make-release'), name: 'sapp-gles-egl-linux-make-release', options: { ...gles, egl: true } });
    c.addConfig({ ...cfg('linux-make-debug'), name: 'sapp-gles-egl-linux-make-debug', options: { ...gles, egl: true } });
    c.addConfig({ ...cfg('linux-ninja-release'), name: 'sapp-gles-egl-linux-ninja-release', options: { ...gles, egl: true } });
    c.addConfig({ ...cfg('linux-ninja-debug'), name: 'sapp-gles-egl-linux-ninja-debug', options: { ...gles, egl: true } });
    c.addConfig({ ...cfg('linux-ninja-release'), name: 'sapp-gles-egl-linux-vscode-release', opener: 'vscode', options: { ...gles, egl: true } });
    c.addConfig({ ...cfg('linux-ninja-debug'), name: 'sapp-gles-egl-linux-vscode-debug', opener: 'vscode', options: { ...gles, egl: true } });

    // linux + vulkan
    c.addConfig({ ...cfg('linux-make-release'), name: 'sapp-vk-linux-make-release', options: vk });
    c.addConfig({ ...cfg('linux-make-debug'), name: 'sapp-vk-linux-make-debug', options: vk });
    c.addConfig({ ...cfg('linux-ninja-release'), name: 'sapp-vk-linux-ninja-release', options: vk });
    c.addConfig({ ...cfg('linux-ninja-debug'), name: 'sapp-vk-linux-ninja-debug', options: vk });
    c.addConfig({ ...cfg('linux-ninja-release'), name: 'sapp-vk-linux-vscode-release', opener: 'vscode', options: vk });
    c.addConfig({ ...cfg('linux-ninja-debug'), name: 'sapp-vk-linux-vscode-debug', opener: 'vscode', options: vk });

    // linux + webgpu
    c.addConfig({ ...cfg('linux-make-release'), name: 'sapp-wgpu-linux-make-release', options: wgpu });
    c.addConfig({ ...cfg('linux-make-debug'), name: 'sapp-wgpu-linux-make-debug', options: wgpu });
    c.addConfig({ ...cfg('linux-ninja-release'), name: 'sapp-wgpu-linux-ninja-release', options: wgpu });
    c.addConfig({ ...cfg('linux-ninja-debug'), name: 'sapp-wgpu-linux-ninja-debug', options: wgpu });
    c.addConfig({ ...cfg('linux-ninja-release'), name: 'sapp-wgpu-linux-vscode-release', opener: 'vscode', options: wgpu });
    c.addConfig({ ...cfg('linux-ninja-debug'), name: 'sapp-wgpu-linux-vscode-debug', opener: 'vscode', options: wgpu });

    // ios + metal
    c.addConfig({
        ...cfg('macos-ninja-release'),
        name: 'sapp-metal-ios-xcode-release',
        platform: 'ios',
        generator: 'xcode',
        opener: 'xcode',
        cmakeCacheVariables: { CMAKE_SYSTEM_NAME: 'iOS' },
        options: metal,
    });
    c.addConfig({
        ...cfg('macos-ninja-debug'),
        name: 'sapp-metal-ios-xcode-debug',
        platform: 'ios',
        generator: 'xcode',
        opener: 'xcode',
        cmakeCacheVariables: { CMAKE_SYSTEM_NAME: 'iOS' },
        options: metal,
    });

    // ios + gles
    c.addConfig({
        ...cfg('macos-ninja-release'),
        name: 'sapp-gles-ios-xcode-release',
        platform: 'ios',
        generator: 'xcode',
        opener: 'xcode',
        cmakeCacheVariables: { CMAKE_SYSTEM_NAME: 'iOS' },
        options: gles,
    });
    c.addConfig({
        ...cfg('macos-ninja-debug'),
        name: 'sapp-gles-ios-xcode-debug',
        platform: 'ios',
        generator: 'xcode',
        opener: 'xcode',
        cmakeCacheVariables: { CMAKE_SYSTEM_NAME: 'iOS' },
        options: gles,
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
    c.addConfig({ ...emsc, name: 'sapp-gles-emsc-make-release', generator: 'make', buildMode: 'release', options: gles });
    c.addConfig({ ...emsc, name: 'sapp-gles-emsc-make-debug', generator: 'make', buildMode: 'debug', options: gles });
    c.addConfig({ ...emsc, name: 'sapp-gles-emsc-ninja-release', generator: 'ninja', buildMode: 'release', options: gles });
    c.addConfig({ ...emsc, name: 'sapp-gles-emsc-ninja-debug', generator: 'ninja', buildMode: 'debug', options: gles });
    c.addConfig({ ...emsc, name: 'sapp-gles-emsc-vscode-release', generator: 'ninja', buildMode: 'release', opener: 'vscode', options: gles });
    c.addConfig({ ...emsc, name: 'sapp-gles-emsc-vscode-debug', generator: 'ninja', buildMode: 'debug', opener: 'vscode', options: gles });

    // emscripten + webgpu
    c.addConfig({ ...emsc, name: 'sapp-wgpu-emsc-make-release', generator: 'make', buildMode: 'release', options: wgpu });
    c.addConfig({ ...emsc, name: 'sapp-wgpu-emsc-make-debug', generator: 'make', buildMode: 'debug', options: wgpu });
    c.addConfig({ ...emsc, name: 'sapp-wgpu-emsc-ninja-release', generator: 'ninja', buildMode: 'release', options: wgpu });
    c.addConfig({ ...emsc, name: 'sapp-wgpu-emsc-ninja-debug', generator: 'ninja', buildMode: 'debug', options: wgpu });
    c.addConfig({ ...emsc, name: 'sapp-wgpu-emsc-vscode-release', generator: 'ninja', buildMode: 'release', opener: 'vscode', options: wgpu });
    c.addConfig({ ...emsc, name: 'sapp-wgpu-emsc-vscode-debug', generator: 'ninja', buildMode: 'debug', opener: 'vscode', options: wgpu });

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
    c.addConfig({ ...android, name: 'sapp-gles-android-make-release', generator: 'make', buildMode: 'release', options: gles });
    c.addConfig({ ...android, name: 'sapp-gles-android-make-debug', generator: 'make', buildMode: 'debug', options: gles });
    c.addConfig({ ...android, name: 'sapp-gles-android-ninja-release', generator: 'ninja', buildMode: 'release', options: gles });
    c.addConfig({ ...android, name: 'sapp-gles-android-ninja-debug', generator: 'ninja', buildMode: 'debug', options: gles });
    c.addConfig({ ...android, name: 'sapp-gles-android-vscode-release', generator: 'ninja', buildMode: 'release', opener: 'vscode', options: gles });
    c.addConfig({ ...android, name: 'sapp-gles-android-vscode-debug', generator: 'ninja', buildMode: 'debug', opener: 'vscode', options: gles });
}

function addGlfwConfigs(c: Configurer) {
    const glfw = { glfw: true, backend: 'glcore' };

    // windows
    c.addConfig({ ...cfg('win-msvc-debug'), name: 'glfw-win-msvc-debug', options: glfw });
    c.addConfig({ ...cfg('win-msvc-release'), name: 'glfw-win-msvc-release', options: glfw });
    c.addConfig({ ...cfg('win-msvc-debug'), name: 'glfw-win-vstudio-debug', opener: 'vstudio', options: glfw });
    c.addConfig({ ...cfg('win-msvc-release'), name: 'glfw-win-vstudio-release', opener: 'vstudio', options: glfw  });
    c.addConfig({ ...cfg('win-msvc-debug'), name: 'glfw-win-vscode-debug', generator: 'ninja', opener: 'vscode', options: glfw });
    c.addConfig({ ...cfg('win-msvc-release'), name: 'glfw-win-vscode-release', generator: 'ninja', opener: 'vscode', options: glfw });

    // macos
    c.addConfig({ ...cfg('macos-make-release'), name: 'glfw-macos-make-release', options: glfw });
    c.addConfig({ ...cfg('macos-make-debug'), name: 'glfw-macos-make-debug', options: glfw });
    c.addConfig({ ...cfg('macos-ninja-release'), name: 'glfw-macos-ninja-release', options: glfw });
    c.addConfig({ ...cfg('macos-ninja-debug'), name: 'glfw-macos-ninja-debug', options: glfw });
    c.addConfig({ ...cfg('macos-ninja-release'), name: 'glfw-macos-vscode-release', opener: 'vscode', options: glfw });
    c.addConfig({ ...cfg('macos-ninja-debug'), name: 'glfw-macos-vscode-debug', opener: 'vscode', options: glfw });
    c.addConfig({ ...cfg('macos-ninja-release'), name: 'glfw-macos-xcode-release', generator: 'xcode', opener: 'xcode', options: glfw });
    c.addConfig({ ...cfg('macos-ninja-debug'), name: 'glfw-macos-xcode-debug', generator: 'xcode', opener: 'xcode', options: glfw });

    // linux
    c.addConfig({ ...cfg('linux-make-release'), name: 'glfw-linux-make-release', options: glfw });
    c.addConfig({ ...cfg('linux-make-debug'), name: 'glfw-linux-make-debug', options: glfw });
    c.addConfig({ ...cfg('linux-ninja-release'), name: 'glfw-linux-ninja-release', options: glfw });
    c.addConfig({ ...cfg('linux-ninja-debug'), name: 'glfw-linux-ninja-debug', options: glfw });
    c.addConfig({ ...cfg('linux-ninja-release'), name: 'glfw-linux-vscode-release', opener: 'vscode', options: glfw });
    c.addConfig({ ...cfg('linux-ninja-debug'), name: 'glfw-linux-vscode-debug', opener: 'vscode', options: glfw });
}
