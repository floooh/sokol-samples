import { type Configurer, type Project, log, util, proj } from 'jsr:@floooh/fibs@^1';
import { green } from 'jsr:@std/fmt@^1/colors';

import { samples, type SampleOptions } from './sapp.ts';

type Api = 'webgl' | 'webgpu';

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
        ],
        title: 'Sokol WebGPU',
        crossName: 'webgl',
        crossUrl: 'https://floooh.github.io/sokol-html5',
        bodyBgColor: '#292d3e',
        thumbBgColor: '#4a4d62',
        textColor: '#3498DB',
    },
};

export function addWebPageCommand(c: Configurer) {
    c.addCommand({ name: 'webpage', help, run });
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

function servePage(p: Project, dstDir: string) {
    const emsc = p.importModule("extras", "emscripten.ts");
    emsc.emrun(p, { cwd: dstDir, file: "index.html" });
}

async function generatePage(p: Project, api: Api, srcDir: string, dstDir: string) {
    const props = Props[api];
    for (const s of samples) {
        if (props.skip.includes(s.name)) {
            log.info(`> skipping sample ${s.name}`);
            continue;
        }
        log.info(`> FIXME: sample ${s.name}`);
    }
}
