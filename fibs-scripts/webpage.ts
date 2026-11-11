// LLM maintained
//
// The 'fibs webpage' command: build the samples webpage from sapp/_toc.yaml
// and the page files in webpage/site.

import { type Configurer, type Project, log, util, proj } from 'jsr:@floooh/fibs@^1';
import { green } from 'jsr:@std/fmt@^1/colors';
import { parse } from 'jsr:@std/yaml@^1';

import { samples as buildSamples } from './sapp.ts';

type Api = 'webgl' | 'webgpu';

// one sample in sapp/_toc.yaml
type TocSample = {
    name: string;
    description: string;
    tags: string[];
    source: string;
    shader?: string;
    ui: boolean;
    webgl: boolean;
    webgpu: boolean;
};

type Toc = {
    assets: string[];
    samples: TocSample[];
};

const ConfigRoots: Record<Api, string> = {
    webgl: 'sapp-gles-emsc-ninja',
    webgpu: 'sapp-wgpu-emsc-ninja',
};

// the page files in webpage/site
const SiteFiles = [
    'index.html',
    'player.html',
    'style.css',
    'common.js',
    'main.js',
    'player.js',
    'highlight.js',
    'favicon.svg',
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
        'webpage build [webgl|webgpu] [--debug]',
        'webpage serve [webgl|webgpu] [--debug]'
    ], 'build or serve samples webpage');
}

function getConfigName(api: Api, debug: boolean): string {
    return `${ConfigRoots[api]}-${debug ? 'debug':'release'}`;
}

async function run(p: Project, args: string[]) {
    const debug = args.includes('--debug');
    if (debug) {
        log.warn('using debug build config!');
    }
    args = args.filter((val) => val != '--debug');
    const subcmd = args[1];
    const api = args[2];
    if ((subcmd !== 'build') && (subcmd !== 'serve')) {
        throw new Error("subcommand 'build' or 'serve' expected (run 'fibs help webpage')");
    }
    if ((api !== 'webgl') && (api !== 'webgpu')) {
        throw new Error ("subcommand argument 'webgl' or 'webgpu' expected (run 'fibs help webpage')");
    }
    if (subcmd === 'build') {
        await buildPage(p, api, debug);
    } else {
        servePage(p, api);
    }
}

function logStatus(status: string, configName: string) {
    log.info(`${green('>>')} ${status} '${configName}' ...`);
}

async function buildPage(p: Project, api: Api, debug: boolean) {
    const configName = getConfigName(api, debug);
    const config = p.config(configName);
    const srcDir = p.distDir(configName);
    const dstDir = `${p.fibsDir()}/webpage/${api}`;
    // check _toc.yaml before the long build
    loadToc(p.dir());
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
    await generatePage(p.dir(), api, srcDir, dstDir);
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

// load sapp/_toc.yaml and check it against fibs-scripts/sapp.ts
function loadToc(rootDir: string): Toc {
    const toc = parse(Deno.readTextFileSync(`${rootDir}/sapp/_toc.yaml`)) as Toc;
    for (const s of toc.samples) {
        const err = (msg: string) => new Error(`sapp/_toc.yaml: sample '${s.name}': ${msg}`);
        const b = buildSamples.find((b) => b.name === s.name);
        if (!b) {
            throw err('not found in fibs-scripts/sapp.ts');
        }
        if (s.ui !== !!b.ui) {
            throw err(`'ui: ${s.ui}' does not match fibs-scripts/sapp.ts`);
        }
        if (s.source !== `${s.name}-sapp.${b.ext ?? 'c'}`) {
            throw err(`'source: ${s.source}' does not match fibs-scripts/sapp.ts`);
        }
        for (const file of [s.source, s.shader]) {
            if (file && !util.fileExists(`${rootDir}/sapp/${file}`)) {
                throw err(`file 'sapp/${file}' not found`);
            }
        }
    }
    return toc;
}

// copy the Emscripten-generated sample page, which runs inside the player iframe
function copySample(srcDir: string, dstDir: string, name: string, ui: boolean) {
    const prog = `${name}-sapp${ui ? '-ui' : ''}`;
    Deno.copyFileSync(`${srcDir}/${prog}.wasm`, `${dstDir}/${prog}.wasm`);
    Deno.copyFileSync(`${srcDir}/${prog}.js`, `${dstDir}/${prog}.js`);
    // use the sample name as page title for the [new tab] link
    const html = Deno.readTextFileSync(`${srcDir}/${prog}.html`)
        .replace(/<title>.*<\/title>/, `<title>${name}${ui ? ' (ui)' : ''}</title>`);
    Deno.writeTextFileSync(`${dstDir}/${prog}.html`, html);
}

export async function generatePage(rootDir: string, api: Api, srcDir: string, dstDir: string) {
    const siteDir = `${rootDir}/webpage/site`;
    const shotDir = `${rootDir}/webpage/screenshots`;
    const toc = loadToc(rootDir);
    const samples = toc.samples.filter((s) => s[api]);

    // samples.json has all samples, the player links samples of the other api to the other page
    log.info('> writing samples.json');
    Deno.writeTextFileSync(`${dstDir}/samples.json`, JSON.stringify({ api, samples: toc.samples }));

    log.info('> copying page files');
    for (const file of SiteFiles) {
        Deno.copyFileSync(`${siteDir}/${file}`, `${dstDir}/${file}`);
    }

    for (const sample of samples) {
        log.info(`> copying sample: ${sample.name}`);
        copySample(srcDir, dstDir, sample.name, false);
        if (sample.ui) {
            copySample(srcDir, dstDir, sample.name, true);
        }
    }

    for (const asset of toc.assets) {
        Deno.copyFileSync(`${srcDir}/${asset}`, `${dstDir}/${asset}`);
    }

    // convert screenshots to webp, the page shows dummy.webp for missing screenshots
    for (const name of [...samples.map((s) => s.name), 'dummy']) {
        const src = `${shotDir}/${name}.jpg`;
        if (util.fileExists(src)) {
            await cwebp(src, `${dstDir}/${name}.webp`);
        } else {
            log.warn(`no screenshot for ${name} (${src})`);
        }
    }
}
