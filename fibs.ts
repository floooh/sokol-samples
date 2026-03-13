import { type Configurer, type Builder, type Project } from 'jsr:@floooh/fibs@^1';

export function configure(c: Configurer) {
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
        ]
    });
    c.addImport({
        name: 'libs',
        url: 'https://github.com/floooh/fibs-libs',
        // FIXME: nuklear, microui, stb, glfw3
        files: ['sokol.ts', 'stb.ts']
    });
    c.addImport({
        name: 'dcimgui',
        url: 'https://github.com/floooh/dcimgui',
        files: ['fibs.ts', 'fibs-docking.ts'],
    });
    c.addImportOptions((_p: Project) => {
        return {
            emscripten: {
                initialMemory: 32 * 1024 * 1024,
                stackSize: 512 * 1024,
            },
            // FIXME: Android import options
        };
    });
}

export function build(_b: Builder) {
    console.log('FIXME: build');
}
