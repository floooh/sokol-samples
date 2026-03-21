import type { TargetJob, Builder  } from 'jsr:@floooh/fibs@^1';

export function copy(srcDir: string, files: string[]): TargetJob {
    return {
        job: 'copyfiles',
        args: { srcDir, files },
    };
}

export function embed(dir: string, outHeader: string, files: string[]): TargetJob {
    return {
        job: 'embedfiles',
        args: { dir, outHeader, files },
    };
}

export type ShdcOptions = {
    name: string,
    defs?: string[]
    mod?: string,
    refl?: boolean
};

export function shdc(opts: ShdcOptions): TargetJob {
    const { name, defs, mod, refl } = opts;
    return {
        job: 'sokolshdc',
        args: {
            src: `${name}-sapp.glsl`,
            out: `${name}-sapp.glsl${mod ? `.${mod}` : ''}.h`,
            defines: defs,
            module: mod,
            reflection: refl,
        },
    };
}

export function hasCompute(b: Builder): boolean {
    const cfg = b.activeConfig();
    if (b.isAndroid()) {
        return true;
    } else if (cfg.options.backend === 'gles3') {
        return false;
    } else if ((cfg.options.backend === 'glcore') && b.isMacOS()) {
        return false;
    } else {
        return true;
    }
}
