import { type Builder } from 'jsr:@floooh/fibs@^1';

export function addLibs(b: Builder) {
    b.addTarget('sokol-static', 'lib', (t) => {
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
    b.addTarget('fileutil', 'lib', (t) => {
        t.setDir('libs/util');
        t.addSource('fileutil.h');
        if (b.isMacOS() || b.isIOS()) {
            t.addSource('fileutil_osx.m');
        } else {
            t.addSource('fileutil.c');
        }
    });
    b.addTarget('basisu', 'lib', (t) => {
        t.setDir('libs/basisu');
        t.addSources(['sokol_basisu.cpp', 'sokol_basisu.h']);
        t.addDependencies(['sokol']);
        if (b.isGcc() || b.isClang()) {
            t.addCompileOptions({
                scope: 'private',
                opts: ['-Wno-unused-value', '-Wno-unused-variable', '-Wno-unused-parameter', '-Wno-type-limits', '-Wno-deprecated-builtins'],
            });
            if (b.isGcc()) {
                t.addCompileOptions({
                    scope: 'private',
                    opts: ['-Wno-maybe-uninitialized', '-Wno-class-memaccess'],
                });
            }
            if (b.isClang()) {
                t.addCompileOptions({
                    scope: 'private',
                    opts: ['-Wno-deprecated-declarations'],
                });
            }
        }
    });
    b.addTarget('qoi', 'lib', (t) => {
        t.setDir('libs/qoi');
        t.addSources(['qoi.c', 'qoi.h']);
        if (b.isGcc() || b.isClang()) {
            t.addCompileOptions({ scope: 'private', opts: ['-Wno-sign-conversion'] });
        } else if (b.isMsvc()) {
            t.addCompileOptions({ scope: 'private', opts: [
                '/wd4244',  // conversion from 'X' to 'Y', possible loss of data
            ]});
        }
    });
    b.addTarget('nuklear-static', 'lib', (t) => {
        t.setDir('libs/nuklear');
        t.addSource('nuklear.c');
        t.addDependencies(['nuklear']);
    });
    b.addTarget('spine', 'lib', (t) => {
        const glob = (dir: string) => {
            const res: string[] = [];
            for (const entry of Deno.readDirSync(dir)) {
                if (entry.isFile && entry.name.endsWith('.c')) {
                    res.push(entry.name);
                }
            }
            return res;
        };
        t.setDir('libs/spine-c/src/spine');
        t.addSources(glob(`${b.selfDir()}/libs/spine-c/src/spine`));
        t.addIncludeDirectories(['../../include']);
        if (b.isMsvc()) {
            t.addCompileOptions({
                scope: 'private',
                opts: [
                    '/wd4127', // conditional expression is constant
                    '/wd4305', // truncation from 'int' to 'float'
                    '/wd4244', // conversion from 'X' to 'Y', possible loss of data
                    '/wd4267', // conversion from 'X' to 'Y', possible loss of data
                ],
            });
        } else if (b.isGcc() || b.isClang()) {
            t.addCompileOptions({
                scope: 'private',
                opts: [
                    '-Wno-sign-conversion',
                    '-Wno-unused-but-set-variable',
                    '-Wno-implicit-fallthrough',
                    '-Wno-parentheses'
                ],
            });
            if (b.isClang()) {
                t.addCompileOptions({
                    scope: 'private',
                    opts: [
                        '-Wno-shorten-64-to-32',
                        '-Wno-implicit-const-int-float-conversion',
                    ],
                });
            }
        }
    });
    b.addTarget('ozzutil', 'lib', (t) => {
        t.setDir('libs/ozzutil');
        t.addSources(['ozzutil.cc', 'ozzutil.h']);
        t.addDependencies(['sokol', 'ozzanimation']);
    });
}
