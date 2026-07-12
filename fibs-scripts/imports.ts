import { type Configurer, type Project, log } from 'jsr:@floooh/fibs@^1';

export function addImports(c: Configurer) {
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
        files: [
            'sokol.ts',
            'stb.ts',
            'libmodplug.ts',
            'ozzanimation.ts',
            'microui.ts',
            'nuklear.ts',
            'glfw3.ts',
            'box3d.ts',
        ],
    });
    c.addImport({
        name: 'dcimgui',
        url: 'https://github.com/floooh/dcimgui',
        files: ['fibs.ts', 'fibs-docking.ts'],
    });
    c.addImportOptions((p: Project) => {
        // temporarily disable Closure pass on Emscripten WebGPU until
        // https://issues.chromium.org/issues/529689760 is fixed
        let emscUseClosure = true;
        if (p.activeConfig().name.includes('wgpu-emsc')) {
            log.warn('disabling Emscripten Closure pass (see: https://issues.chromium.org/issues/529689760)');
            emscUseClosure = false;
        }
        return {
            sokol: {
                backend: p.activeConfig().options.backend,
                useEGL: p.activeConfig().options.egl,
            },
            emscripten: {
                useClosure: emscUseClosure,
            }
        };
    });
}
