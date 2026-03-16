import { type Configurer, type Project } from 'jsr:@floooh/fibs@^1';

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
        ],
    });
    c.addImport({
        name: 'dcimgui',
        url: 'https://github.com/floooh/dcimgui',
        files: ['fibs.ts', 'fibs-docking.ts'],
    });
    c.addImportOptions((p: Project) => {
        return {
            sokol: {
                backend: p.activeConfig().options.backend,
                useEGL: p.activeConfig().options.egl,
            },
            // FIXME: Android import options
        };
    });
}
