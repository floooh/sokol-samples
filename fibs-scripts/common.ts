import type { Config, TargetJob, Builder  } from 'jsr:@floooh/fibs@^1';

export function sokolBackendByConfig(config: Config) {
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

export function copy(srcDir: string, files: string[]): TargetJob[] {
    return [{
        job: 'copyfiles',
        args: { srcDir, files },
    }];
}

export function embed(dir: string, outHeader: string, files: string[]): TargetJob[] {
    return [{
        job: 'embedfiles',
        args: { dir, outHeader, files },
    }];
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
    } else if (sokolBackendByConfig(cfg) === 'gles3') {
        return false;
    } else if ((sokolBackendByConfig(cfg) === 'glcore') && b.isMacOS()) {
        return false;
    } else {
        return true;
    }
}
