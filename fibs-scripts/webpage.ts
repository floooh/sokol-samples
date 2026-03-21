import { type Configurer, type Project, log, util, proj } from 'jsr:@floooh/fibs@^1';
import { green } from 'jsr:@std/fmt@^1/colors';

import { samples, type SampleOptions } from './sapp.ts';

type Api = 'webgl' | 'webgpu';

const GitHubSamplesUrl = 'https://github.com/floooh/sokol-samples/tree/master/sapp'

const Props = {
    webgl: {
        config: 'sapp-gles-emsc-ninja-release',
        skip: [
            'sbufoffset',
            'instancing-compute',
            'computeboids',
            'write-storageimage',
            'imageblur',
            'imgui-perf',
            'noentry-dll',
        ],
        title: 'Sokol WebGL',
        crossName: 'webgpu',
        crossUrl: 'https://floooh.github.io/sokol-webgpu',
        bodyBgColor: '#292d3e',
        thumbBgColor: '#4a4d62',
        textColor: '#1ABC9C',
    },
    webgpu: {
        config: 'sapp-wgpu-emsc-ninja-release',
        skip: [
            'imgui-perf',
            'noentry-dll',
        ],
        title: 'Sokol WebGPU',
        crossName: 'webgl',
        crossUrl: 'https://floooh.github.io/sokol-html5',
        bodyBgColor: '#292d3e',
        thumbBgColor: '#4a4d62',
        textColor: '#3498DB',
    },
};

const assets = [
    "comsi.s3m",
    "baboon.png",
    "bjork-all-is-full-of-love.mpg",
    "DamagedHelmet.gltf",
    "DamagedHelmet.bin",
    "Default_AO.basis",
    "Default_albedo.basis",
    "Default_emissive.basis",
    "Default_metalRoughness.basis",
    "Default_normal.basis",
    "DroidSerif-Regular.ttf",
    "DroidSerif-Italic.ttf",
    "DroidSerif-Bold.ttf",
    "DroidSansJapanese.ttf",
    "ozz_anim_skeleton.ozz",
    "ozz_anim_animation.ozz",
    "ozz_skin_skeleton.ozz",
    "ozz_skin_animation.ozz",
    "ozz_skin_mesh.ozz",
    "raptor-pma.atlas",
    "raptor-pma.png",
    "raptor-pro.skel",
    "spineboy-pro.json",
    "spineboy.atlas",
    "spineboy.png",
    "alien-pma.atlas",
    "alien-pma.png",
    "alien-pro.skel",
    "speedy-pma.atlas",
    "speedy-pma.png",
    "speedy-ess.skel",
    "mix-and-match-pma.atlas",
    "mix-and-match-pma.png",
    "mix-and-match-pro.skel",
    "nb2_posx.jpg",
    "nb2_negx.jpg",
    "nb2_posy.jpg",
    "nb2_negy.jpg",
    "nb2_posz.jpg",
    "nb2_negz.jpg",
    "kodim05.basis",
    "kodim07.basis",
    "kodim17.basis",
    "kodim20.basis",
    "kodim23.basis",
    "dice.qoi",
];

export function addWebPageCommand(c: Configurer) {
    c.addCommand({ name: 'webpage', help, run });
    c.addTool({
        name: 'cwebp',
        optional: true,
        notFoundMsg: `needed for building samples webpage`,
        platforms: ['windows', 'linux', 'macos'],
        exists: cwebpExists,
    })
}

function help() {
    log.helpCmd([
        'webpage build [webgl|webgpu]',
        'webpage serve [webgl|webgpu]'
    ], 'build or serve samples webpage');
}

async function run(p: Project, args: string[]) {
    const subcmd = args[1];
    const api = args[2];
    if ((subcmd !== 'build') && (subcmd !== 'serve')) {
        throw new Error("subcommand 'build' or 'serve' expected (run 'fibs help webpage')");
    }
    if ((api !== 'webgl') && (api !== 'webgpu')) {
        throw new Error ("subcommand argument 'webgl' or 'webgpu' expected (run 'fibs help webpage')");
    }
    if (subcmd === 'build') {
        await buildPage(p, api);
    } else {
        servePage(p, api);
    }
}

function logStatus(status: string, configName: string) {
    log.info(`${green('>>')} ${status} '${configName}' ...`);
}

async function buildPage(p: Project, api: Api) {
    const configName = Props[api].config;
    const config = p.config(configName);
    const srcDir = p.distDir(configName);
    const dstDir = `${p.fibsDir()}/webpage/${api}`;
    if (util.dirExists(dstDir)) {
        if (Deno.env.get('CI') || log.ask(`Ok to delete directory ${dstDir}?`, false)) {
            Deno.removeSync(dstDir, { recursive: true });
        }
    }
    util.ensureDir(dstDir);
    logStatus('configuring', configName);
    await proj.generate(config);
    logStatus('building', configName);
    await proj.build({});
    logStatus('genering webpage for', configName);
    await generatePage(p, api, srcDir, dstDir);
}

function servePage(p: Project, api: string) {
    // FIXME: emrun doesn't seem to support HTTP range requests
    // switch to node http-server instead
    const dstDir = `${p.fibsDir()}/webpage/${api}`;
    const emsc = p.importModule("extras", "emscripten.ts");
    emsc.emrun(p, { cwd: dstDir, file: "index.html" });
}

async function cwebpExists(): Promise<boolean> {
    try {
        await util.runCmd('cwebp', {
            args: ['-version'],
            stdout: 'piped',
            stderr: 'piped',
            showCmd: false,
        });
        return true;
    } catch (_err) {
        return false;
    }
}

async function cwebp(srcPath: string, dstPath: string) {
    await util.runCmd('cwebp', { args: [ '-quiet', '-q', '80', srcPath, '-o', dstPath ], showCmd: true });
}

async function generatePage(p: Project, api: Api, srcDir: string, dstDir: string) {
    const webAssetDir = `${p.dir()}/webpage`;
    const props = Props[api];

    // filter samples by WebGL2 vs WebGPU
    const filteredSamples = samples.filter((s) => !props.skip.includes(s.name));

    // build thumbnail gallery
    let content = '';
    for (const sample of filteredSamples) {
        const name = sample.name;
        log.info(`> adding thumbnail for ${name}`);
        const url = `${name}-sapp.html`;
        const uiUrl = `${name}-sapp-ui.html`;
        let imgFilename = `${name}.webp`;
        if (!util.fileExists(`${webAssetDir}/${name}.jpg`)) {
            imgFilename = `dummy.webp`;
        }
        content += '<div class="thumb">';
        content += `  <div class="thumb-title">${name}</div>`
        if (sample.ui) {
            content += `<a class="img-btn-link" href="${uiUrl}"><div class="img-btn">UI</div></a>`;
        }
        content += `  <div class="img-frame"><a href="${url}"><img class="image" src="${imgFilename}"></img></a></div>`;
        content += '</div>\n'
    }

    // populate html template and write to dstDir
    log.info('> writing index.html');
    const indexTmpl = Deno.readTextFileSync(`${webAssetDir}/index.html`);
    const indexHtml = indexTmpl.replaceAll('${samples}', content)
        .replaceAll('${title}', props.title)
        .replaceAll('${cross_name}', props.crossName)
        .replaceAll('${cross_url}', props.crossUrl)
        .replaceAll('${body_bg_color}', props.bodyBgColor)
        .replaceAll('${thumb_bg_color}', props.thumbBgColor)
        .replaceAll('${text_color}', props.textColor);
    Deno.writeTextFileSync(`${dstDir}/index.html`, indexHtml, {});

    // copy additional webpage asset files
    Deno.copyFileSync(`${webAssetDir}/favicon.png`, `${dstDir}/favicon.png`);

    // generate per-sample .html and copy .wasm + .js
    for (const sample of filteredSamples) {
        const name = sample.name;
        log.info(`> generate html page: ${name}`);
        generateSample(p, sample, srcDir, dstDir, '-sapp');
        if (sample.ui) {
            generateSample(p, sample, srcDir, dstDir, '-sapp-ui');
        }
        if (sample.ui) {
            Deno.copyFileSync(`${srcDir}/${name}-sapp-ui.wasm`, `${dstDir}/${name}-sapp-ui.wasm`);
            Deno.copyFileSync(`${srcDir}/${name}-sapp-ui.js`, `${dstDir}/${name}-sapp-ui.js`);
        }
    }

    // copy additional assets
    for (const asset of assets) {
        Deno.copyFileSync(`${srcDir}/${asset}`, `${dstDir}/${asset}`);
    }

    // convert screenshots to webp
    for (const sample of filteredSamples) {
        await cwebp(`${webAssetDir}/${sample.name}.jpg`, `${dstDir}/${sample.name}.webp`);
    }
}

function generateSample(p: Project, sample: SampleOptions, srcDir: string, dstDir: string, postfix: string) {
    const webAssetDir = `${p.dir()}/webpage`;
    const name = sample.name;
    Deno.copyFileSync(`${srcDir}/${name}${postfix}.wasm`, `${dstDir}/${name}${postfix}.wasm`);
    Deno.copyFileSync(`${srcDir}/${name}${postfix}.js`, `${dstDir}/${name}${postfix}.js`);

    const srcUrl = `${GitHubSamplesUrl}/${name}-sapp.${sample.ext ?? 'c'}`;
    const glslUrl = sample.shd ? `${GitHubSamplesUrl}/${name}${postfix}.glsl` : null;
    const sampleTmpl = Deno.readTextFileSync(`${webAssetDir}/wasm.html`);
    const sampleHtml = sampleTmpl.replaceAll('${name}', name)
        .replaceAll('${prog}', `${name}${postfix}`)
        .replaceAll('${source}', srcUrl)
        .replaceAll('${glsl}', (glslUrl !== null) ? glslUrl : '')
        .replaceAll('${hidden}', (glslUrl === null) ? 'hidden' : '');
    Deno.writeTextFileSync(`${dstDir}/${name}${postfix}.html`, sampleHtml, {});
}
