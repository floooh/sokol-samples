import { type Configurer, type ConfigDesc, conf, util } from 'jsr:@floooh/fibs@^1';

const ANDROID_ABI = 'armeabi-v7a';
const ANDROID_PLATFORM = 'android-30';

export function addConfigs(c: Configurer) {
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
