// LLM maintained
//
// Preview the samples webpage without building the samples.
//
// Usage (from the project root):
//
//   deno run -A webpage/tools/preview.ts [webgl|webgpu] [--port=8000] [--dist=dir]
//
// Writes samples.json from sapp/_toc.yaml and converts the screenshots
// to .webp (needs cwebp) into .fibs/webpage-preview/[api]. The page files
// are served directly from webpage/site.
// Files which are not found there are served from the --dist directory,
// for instance .fibs/dist/sapp-gles-emsc-vscode-debug, so that the player
// can run the built samples. Without --dist, the player shows a 404 page.
import { parse } from 'jsr:@std/yaml@^1';
import { serveDir } from 'jsr:@std/http@^1/file-server';

const api = Deno.args.find((a) => !a.startsWith('--')) ?? 'webgl';
if ((api !== 'webgl') && (api !== 'webgpu')) {
    throw new Error("expected 'webgl' or 'webgpu'");
}
const portArg = Deno.args.find((a) => a.startsWith('--port='));
const port = portArg ? Number(portArg.split('=')[1]) : 8000;
const distArg = Deno.args.find((a) => a.startsWith('--dist='));
const distDir = distArg ? distArg.slice('--dist='.length) : null;

const rootDir = new URL('../..', import.meta.url).pathname;
const siteDir = `${rootDir}/webpage/site`;
const shotDir = `${rootDir}/webpage/screenshots`;
const dstDir = `${rootDir}/.fibs/webpage-preview/${api}`;
Deno.mkdirSync(dstDir, { recursive: true });

// samples.json
const toc = parse(Deno.readTextFileSync(`${rootDir}/sapp/_toc.yaml`)) as { samples: { name: string }[] };
Deno.writeTextFileSync(`${dstDir}/samples.json`, JSON.stringify({ api, samples: toc.samples }));

// screenshots, only convert when missing
function exists(path: string): boolean {
    try {
        Deno.statSync(path);
        return true;
    } catch {
        return false;
    }
}
for (const name of [...toc.samples.map((s) => s.name), 'dummy']) {
    const src = `${shotDir}/${name}.jpg`;
    const dst = `${dstDir}/${name}.webp`;
    if (exists(src) && !exists(dst)) {
        const cmd = new Deno.Command('cwebp', { args: ['-quiet', '-q', '80', src, '-o', dst] });
        const { success } = await cmd.output();
        if (!success) {
            throw new Error(`cwebp failed for ${src}`);
        }
    }
}

// serve the page files from webpage/site, so that edits show on reload
const siteFiles = ['/', '/index.html', '/player.html', '/style.css', '/common.js', '/main.js', '/player.js', '/favicon.svg'];
Deno.serve({ port }, async (req) => {
    const fsRoot = siteFiles.includes(new URL(req.url).pathname) ? siteDir : dstDir;
    const resp = await serveDir(req, { fsRoot, quiet: true });
    if ((resp.status === 404) && distDir) {
        return serveDir(req, { fsRoot: distDir, quiet: true });
    }
    return resp;
});
