import type { Builder, TargetBuilder, TargetJob } from 'jsr:@floooh/fibs@^1';
import { hasCompute, copy, embed, shdc } from './common.ts';

export function addSokolAppSamples(b: Builder) {
    samples.forEach((s) => addSample(b, s));
}

export const samples: SampleOptions[] = [
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
    { name: 'sbufoffset', ui: 'cc', shd: true, filter: hasCompute },
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
        deps: ['stb', 'fileutil'],
        jobs: [copy('data/nissibeach2', ['nb2_negx.jpg', 'nb2_negy.jpg', 'nb2_negz.jpg', 'nb2_posx.jpg', 'nb2_posy.jpg', 'nb2_posz.jpg'])],
    },
    {
        name: 'basisu',
        ui: 'cc',
        deps: ['basisu'],
        jobs: [embed('data', 'basisu-assets.h', ['basisu/testcard.basis', 'basisu/testcard_rgba.basis'])]
    },
    {
        name: 'blend-playground',
        shd: true,
        deps: ['imgui', 'fileutil', 'qoi'],
        jobs: [copy('data/qoi', ['baboon.qoi', 'dice.qoi', 'testcard_rgba.qoi', 'testcard.qoi'])],
    },
    { name: 'blend', ui: 'cc', shd: true },
    { name: 'blend-op', ui: 'cc', shd: true },
    { name: 'uvwrap', ui: 'cc', shd: true },
    { name: 'uniformtypes', ui: 'cc', shd: true },
    { name: 'imgui', ext: 'cc', sokol: 'cc', deps: ['imgui'] }, // use sokol-cpp for testing, not required though
    { name: 'imgui-dock', ext: 'cc', deps: ['imgui-docking'] },
    { name: 'imgui-highdpi', ext: 'cc', deps: ['imgui'] },
    { name: 'cimgui', deps: ['imgui'] },
    { name: 'imgui-usercallback', shd: true, deps: ['imgui'] },
    { name: 'imgui-images', deps: ['imgui'] },
    { name: 'imgui-perf', deps: ['imgui'] },
    { name: 'events', ext: 'cc', deps: ['imgui'] },
    { name: 'pixelformats', shd: true, deps: ['imgui'] },
    {
        name: 'texview',
        shd: true,
        deps: ['imgui', 'fileutil', 'basisu'],
        jobs: [copy('data/texview', ['kodim05.basis', 'kodim07.basis', 'kodim17.basis', 'kodim20.basis', 'kodim23.basis' ])],
    },
    { name: 'letterbox', deps: ['imgui'] },
    { name: 'framebuffer', deps: ['imgui'] },
    { name: 'sgl', ui: 'cc' },
    { name: 'sgl-lines', ui: 'cc' },
    { name: 'sgl-points', ui: 'cc' },
    { name: 'sgl-context', ui: 'cc' },
    { name: 'sgl-microui', ui: 'c', deps: ['microui'] },
    { name: 'nuklear', ui: 'cc', deps: ['nuklear-static'] },
    { name: 'nuklear-images', ui: 'cc', deps: ['nuklear-static'] },
    { name: 'cubemaprt', ui: 'cc', shd: true },
    { name: 'miprender', ui: 'cc', shd: true },
    { name: 'layerrender', ui: 'cc', shd: true },
    { name: 'sdf', ui: 'cc', shd: true },
    { name: 'shapes', ui: 'cc', shd: true },
    { name: 'shapes-transform', ui: 'cc', shd: true },
    { name: 'primtypes', ui: 'cc', shd: true },
    { name: 'drawcallperf', shd: true, deps: ['imgui'] },
    { name: 'debugtext', ui: 'cc' },
    { name: 'debugtext-printf', ui: 'cc' },
    { name: 'debugtext-userfont', ui: 'cc' },
    { name: 'debugtext-context', shd: true, ui: 'cc' },
    { name: 'debugtext-layers', ui: 'cc' },
    { name: 'saudio' },
    { name: 'icon' },
    { name: 'cursor', ext: 'cc', deps: ['imgui'] },
    { name: 'droptest', deps: ['imgui'] },
    {
        name: 'fontstash',
        ui: 'cc',
        deps: ['fileutil'],
        jobs: [copy('data', ['DroidSansJapanese.ttf', 'DroidSerif-Bold.ttf', 'DroidSerif-Italic.ttf', 'DroidSerif-Regular.ttf'])],
    },
    {
        name: 'fontstash-layers',
        ui: 'cc',
        shd: true,
        deps: ['fileutil'],
        jobs: [copy('data', ['DroidSansJapanese.ttf', 'DroidSerif-Bold.ttf', 'DroidSerif-Italic.ttf', 'DroidSerif-Regular.ttf'])],
    },
    { name: 'modplay', deps: ['libmodplug'], jobs: [embed('data', 'mods.h', ['disco_feva_baby.s3m'])] },
    { name: 'restart', shd: true, deps: ['libmodplug', 'fileutil', 'stb'], jobs: [copy('data', ['baboon.png', 'comsi.s3m'])] },
    {
        name: 'plmpeg',
        shd: true,
        ui: 'cc',
        deps: ['fileutil'],
        jobs: [copy('data', ['bjork-all-is-full-of-love.mpg'])]
    },
    {
        name: 'cgltf',
        shd: true,
        ui: 'cc',
        deps: ['fileutil', 'basisu'],
        jobs: [
            copy('data/gltf/DamagedHelmet', [
                'DamagedHelmet.bin',
                'DamagedHelmet.gltf',
                'Default_albedo.basis',
                'Default_AO.basis',
                'Default_emissive.basis',
                'Default_metalRoughness.basis',
                'Default_normal.basis',
            ]),
        ],
    },
    { name: 'loadpng', shd: true, ui: 'cc', deps: ['fileutil', 'stb'], jobs: [copy('data', ['baboon.png'])] },
    { name: 'spine-simple', ui: 'cc', deps: ['spine', 'stb', 'fileutil'], jobs: [copySpineAssets()] },
    { name: 'spine-inspector', deps: ['spine', 'stb', 'fileutil', 'imgui'], jobs: [copySpineAssets()] },
    { name: 'spine-skinsets', ui: 'cc', deps: ['spine', 'stb', 'fileutil'], jobs: [copySpineAssets()] },
    { name: 'spine-layers', ui: 'cc', deps: ['spine', 'stb', 'fileutil'], jobs: [copySpineAssets()] },
    { name: 'spine-contexts', ui: 'cc', deps: ['spine', 'stb', 'fileutil'], jobs: [copySpineAssets()] },
    { name: 'spine-switch-skinsets', ui: 'cc', deps: ['spine', 'stb', 'fileutil'], jobs: [copySpineAssets()] },
    {
        name: 'ozz-anim',
        ext: 'cc',
        deps: ['fileutil', 'ozzanimation', 'imgui'],
        jobs: [copy('data/ozz', ['ozz_anim_animation.ozz', 'ozz_anim_skeleton.ozz'])],
    },
    {
        name: 'ozz-skin',
        ext: 'cc',
        shd: true,
        deps: ['fileutil', 'ozzanimation', 'imgui'],
        jobs: [copy('data/ozz', ['ozz_skin_animation.ozz', 'ozz_skin_skeleton.ozz', 'ozz_skin_mesh.ozz'])],
    },
    {
        name: 'ozz-storagebuffer',
        ext: 'cc',
        shd: true,
        deps: ['fileutil', 'ozzanimation', 'imgui'],
        jobs: [copy('data/ozz', ['ozz_skin_animation.ozz', 'ozz_skin_skeleton.ozz', 'ozz_skin_mesh.ozz'])],
    },
    {
        name: 'shdfeatures',
        deps: ['fileutil', 'ozzutil', 'imgui'],
        jobs: [
            shdc({ name: 'shdfeatures', mod: 'none', refl: true, defs: ['NONE']}),
            shdc({ name: 'shdfeatures', mod: 'slm', refl: true, defs: ['SKINNING', 'LIGHTING', 'MATERIAL']}),
            shdc({ name: 'shdfeatures', mod: 'sl', refl: true, defs: ['SKINNING', 'LIGHTING']}),
            shdc({ name: 'shdfeatures', mod: 's', refl: true, defs: ['SKINNING']}),
            shdc({ name: 'shdfeatures', mod: 'sm', refl: true, defs: ['SKINNING', 'MATERIAL']}),
            shdc({ name: 'shdfeatures', mod: 'lm', refl: true, defs: ['LIGHTING', 'MATERIAL']}),
            shdc({ name: 'shdfeatures', mod: 'm', refl: true, defs: ['MATERIAL']}),
            shdc({ name: 'shdfeatures', mod: 'l', refl: true, defs: ['LIGHTING']}),
            copy('data/ozz', ['ozz_skin_animation.ozz', 'ozz_skin_skeleton.ozz', 'ozz_skin_mesh.ozz']),
        ],
    },
    { name: 'noentry', sokol: 'noentry', shd: true, filter: (b) => !b.isAndroid() },
    { name: 'noentry-dll', sokol: 'dll', shd: true, filter: (b) => b.isWindows() || b.isMacOS() || b.isLinux() },
    { name: 'instancing-compute', ui: 'cc', shd: true, filter: hasCompute },
    { name: 'write-storageimage', ui: 'cc', shd: true, filter: hasCompute },
    { name: 'computeboids', shd: true, deps: ['imgui'], filter: hasCompute },
    {
        name: 'imageblur',
        shd: true,
        deps: ['imgui', 'stb', 'fileutil'],
        filter: hasCompute,
        jobs: [copy('data', ['baboon.png'])],
    },
];

export type SampleOptions = {
    name: string;
    ext?: 'cc';
    ui?: 'c' | 'cc';
    sokol?: 'cc' | 'dll' | 'noentry';
    shd?: true;
    deps?: string[];
    jobs?: TargetJob[];
    filter?: (b: Builder) => boolean;
};

function addSample(b: Builder, { name, ext, ui, sokol, shd, deps, jobs, filter }: SampleOptions) {
    if (filter && !filter(b)) {
        return;
    }
    const cmn = (t: TargetBuilder) => {
        t.setDir('sapp');
        t.addSource(`${name}-sapp.${ext ?? 'c'}`);
        t.addIncludeDirectories({ system: true, dirs: ['../libs']});
        t.addIncludeDirectories([t.buildDir()]);
        if (sokol === 'cc') {
            t.addDependencies(['sokol-cpp-lib']);
        } else if (sokol === 'noentry') {
            t.addDependencies(['sokol-noentry']);
        } else if (sokol === 'dll') {
            t.addDependencies(['sokol-dll']);
        } else {
            t.addDependencies(['sokol-static']);
        }
        if (deps) {
            t.addDependencies(deps);
        }
        if (shd) {
            t.addSource(`${name}-sapp.glsl`);
            t.addJob(shdc({ name }));
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

function copySpineAssets(): TargetJob {
    return copy('data/spine', [
        'spineboy-pro.json',
        'spineboy.atlas',
        'spineboy.png',
        'raptor-pro.skel',
        'raptor-pma.atlas',
        'raptor-pma.png',
        'alien-pro.skel',
        'alien-pma.atlas',
        'alien-pma.png',
        'speedy-ess.skel',
        'speedy-pma.atlas',
        'speedy-pma.png',
        'mix-and-match-pro.skel',
        'mix-and-match-pma.atlas',
        'mix-and-match-pma.png',
    ]);
}
