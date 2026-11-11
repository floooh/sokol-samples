// LLM maintained
//
// Test the samples webpage in a headless Chrome.
//
// Usage (from the project root, with the preview servers running):
//
//   deno run -A webpage/tools/preview.ts webgl --port=8000
//   deno run -A webpage/tools/preview.ts webgpu --port=8001
//   deno run -A webpage/tools/test.ts [url...]
//
// Default urls are http://localhost:8000/ and http://localhost:8001/.
// Expected results are computed from the samples.json of each page.
// Set CHROME to use a different Chrome executable.
/// <reference lib="dom" />
// (dom types for the callbacks which run in the browser)
import puppeteer, { type Page } from 'npm:puppeteer-core@23';

type Sample = { name: string; description: string; tags: string[]; source: string; shader?: string; ui: boolean; webgl: boolean; webgpu: boolean };
type Toc = { api: 'webgl' | 'webgpu'; samples: Sample[] };

const rootDir = new URL('../..', import.meta.url).pathname;
const githubRaw = 'https://raw.githubusercontent.com/floooh/sokol-samples/master/sapp';
const urls = Deno.args.length > 0 ? Deno.args : ['http://localhost:8000/', 'http://localhost:8001/'];
const chrome = Deno.env.get('CHROME') ?? '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome';

let numFailed = 0;

// show the scrollbars (puppeteer hides them by default), the tests check their width
function launch() {
    return puppeteer.launch({ executablePath: chrome, headless: true, ignoreDefaultArgs: ['--hide-scrollbars'] });
}

function check(what: string, ok: boolean, details?: unknown) {
    if (ok) {
        console.log(`  ok    ${what}`);
    } else {
        numFailed += 1;
        console.log(`  FAIL  ${what}`, details ?? '');
    }
}

function sameList(a: string[], b: string[]): boolean {
    return (a.length === b.length) && a.every((v, i) => v === b[i]);
}

// the search rules from main.js, as the expected behavior
function expected(samples: Sample[], query: string): string[] {
    const terms = query.toLowerCase().split(/[\s,]+/).filter((t) => t.replace(/^#/, '').length > 0);
    return samples.filter((s) => terms.every((t) => {
        if (t.startsWith('#')) {
            return s.tags.some((tag) => tag.includes(t.slice(1)));
        }
        return s.name.includes(t) || s.tags.some((tag) => tag.includes(t));
    })).map((s) => s.name);
}

function visibleCards(page: Page): Promise<string[]> {
    return page.$$eval('.card:not([hidden]) .card-title', (els) => els.map((e) => e.textContent ?? ''));
}

function countText(page: Page): Promise<string> {
    return page.$eval('#count', (e) => e.textContent ?? '');
}

async function search(page: Page, query: string) {
    await page.focus('#search');
    await page.keyboard.press('Escape');
    await page.keyboard.type(query);
}

async function checkSearch(page: Page, samples: Sample[], query: string) {
    await search(page, query);
    const want = expected(samples, query);
    const got = await visibleCards(page);
    check(`search '${query}' shows ${want.length} samples`, sameList(got, want), { want, got });
    check(`search '${query}' count text`, (await countText(page)) === `[${want.length}/${samples.length}]`, await countText(page));
    const msgHidden = await page.$eval('#message', (e) => (e as HTMLElement).hidden);
    check(`search '${query}' no-match message`, msgHidden === (want.length > 0));
}

async function testPage(url: string) {
    console.log(`testing ${url}`);
    const browser = await launch();
    try {
        const page = await browser.newPage();
        const errors: string[] = [];
        page.on('console', (m) => {
            if (m.type() === 'error') {
                errors.push(m.text());
            }
        });
        page.on('pageerror', (e) => errors.push(String(e)));
        let numTocRequests = 0;
        page.on('request', (r) => {
            if (r.url().endsWith('/samples.json')) {
                numTocRequests += 1;
            }
        });

        const toc: Toc = await (await fetch(new URL('samples.json', url))).json();
        const samples = toc.samples.filter((s) => s[toc.api]);

        await page.goto(url, { waitUntil: 'networkidle0' });
        check('samples.json loaded once', numTocRequests === 1, numTocRequests);

        // initial state
        const apiTag = (toc.api === 'webgl') ? 'webgl2' : 'webgpu';
        check('api badge', (await page.$eval('#api-badge', (e) => e.textContent)) === apiTag);
        check('api color scheme', (await page.evaluate(() => document.documentElement.dataset.api)) === toc.api);
        check('all samples shown', sameList(await visibleCards(page), samples.map((s) => s.name)));
        check('count text', (await countText(page)) === `[${samples.length}/${samples.length}]`);
        const numUi = await page.$$eval('.ui-btn', (els) => els.length);
        check('UI buttons', numUi === samples.filter((s) => s.ui).length, numUi);
        const first = samples[0].name;
        check('card links to player', (await page.$eval('.card-title a', (e) => e.getAttribute('href'))) === `player.html?name=${first}`);
        const firstUi = samples.find((s) => s.ui)!.name;
        check('UI button links to player', (await page.$eval('.ui-btn', (e) => e.getAttribute('href'))) === `player.html?name=${firstUi}&ui=1`);

        // '/' focuses the search field
        await page.click('body');
        await page.keyboard.press('/');
        check("'/' focuses search", (await page.evaluate(() => document.activeElement?.id)) === 'search');

        // search
        for (const query of ['cube', 'sgl', '#sgl', '#compute', '#sokol_gl lines', 'texture msaa', '#', 'xyzzy']) {
            await checkSearch(page, samples, query);
        }

        // matching tags are highlighted
        await search(page, '#sokol_gl');
        const matched = await page.$$eval('.card:not([hidden]) .tag.match', (els) => [...new Set(els.map((e) => e.textContent))]);
        check('matching tags highlighted', sameList(matched, ['sokol_gl']), matched);

        // query is stored in the url
        check('query in url', new URL(page.url()).searchParams.get('q') === '#sokol_gl');

        // Escape clears the search
        await page.keyboard.press('Escape');
        check('Escape clears search', (await page.$eval('#search', (e) => (e as HTMLInputElement).value)) === '');
        check('Escape shows all samples', (await visibleCards(page)).length === samples.length);
        check('Escape clears url query', new URL(page.url()).searchParams.get('q') === null);

        // the clear button is only visible with a query, and clears the search
        const clearVisible = () => page.$eval('#search-clear', (e) => !(e as HTMLElement).hidden);
        check('clear button hidden when empty', !(await clearVisible()));
        await search(page, 'cube');
        check('clear button visible with query', await clearVisible());
        await page.click('#search-clear');
        check('clear button clears search', (await page.$eval('#search', (e) => (e as HTMLInputElement).value)) === '');
        check('clear button shows all samples', (await visibleCards(page)).length === samples.length);
        check('clear button focuses search', (await page.evaluate(() => document.activeElement?.id)) === 'search');
        check('clear button hides itself', !(await clearVisible()));

        // clicking a tag searches for '#tag'
        const tag = await page.$eval('.card .tag', (e) => e.textContent ?? '');
        await page.click('.card .tag');
        check(`clicking tag searches '#${tag}'`, (await page.$eval('#search', (e) => (e as HTMLInputElement).value)) === `#${tag}`);
        check(`clicking tag filters`, sameList(await visibleCards(page), expected(samples, `#${tag}`)));

        // Escape works when the search field is not focused, and focuses it
        await page.keyboard.press('Escape');
        check('Escape outside search clears it', (await page.$eval('#search', (e) => (e as HTMLInputElement).value)) === '');
        check('Escape outside search focuses it', (await page.evaluate(() => document.activeElement?.id)) === 'search');
        await page.keyboard.type('cube');
        check('typing after Escape searches', sameList(await visibleCards(page), expected(samples, 'cube')));

        // a query in the url is applied on load
        await page.goto(`${url}?q=${encodeURIComponent('#spine')}`, { waitUntil: 'networkidle0' });
        check('url query applied on load', sameList(await visibleCards(page), expected(samples, '#spine')));

        // the logo links to the sokol repo
        const logoHref = await page.$eval('.logo-link', (e) => (e as HTMLAnchorElement).href);
        check('logo links to sokol repo', logoHref === 'https://github.com/floooh/sokol', logoHref);

        // theme toggle, persists after reload
        const theme = () => page.evaluate(() => document.documentElement.dataset.theme);
        const before = await theme();
        check('default theme is dark', before === 'dark', before);
        await page.click('#theme-btn');
        const after = await theme();
        check('theme toggles', (before !== after) && ((after === 'dark') || (after === 'light')), { before, after });
        await page.reload({ waitUntil: 'networkidle0' });
        check('theme persists after reload', (await theme()) === after);

        // the browser controls follow the page theme, not the OS theme
        const colorScheme = () => page.evaluate(() => getComputedStyle(document.documentElement).colorScheme);
        check('color-scheme follows theme', (await colorScheme()) === after, await colorScheme());
        await page.click('#theme-btn');
        check('color-scheme follows theme toggle', (await colorScheme()) === before, await colorScheme());

        // themed scrollbars via ::-webkit-scrollbar (12px), scrollbar-color is only set
        // for Firefox, else Chrome ignores ::-webkit-scrollbar
        const scrollbarWidth = await page.evaluate(() => innerWidth - document.documentElement.clientWidth);
        check('themed scrollbar width', scrollbarWidth === 12, scrollbarWidth);
        const scrollbarColor = await page.evaluate(() => getComputedStyle(document.documentElement).scrollbarColor);
        check('no scrollbar-color in Chrome', scrollbarColor === 'auto', scrollbarColor);

        check('no console errors', errors.length === 0, errors);
    } finally {
        await browser.close();
    }
}

// the [kind, text] list of the highlighted tokens of a code snippet
function tokens(page: Page, code: string, lang: string): Promise<string[][]> {
    return page.evaluate((code, lang) => {
        // @ts-ignore: highlight() is in highlight.js
        const frag: DocumentFragment = highlight(code, lang);
        return [...frag.childNodes].filter((n) => n.nodeType === Node.ELEMENT_NODE)
            .map((n) => [(n as HTMLElement).className.slice('hl-'.length), n.textContent ?? '']);
    }, code, lang);
}

async function checkTokens(page: Page, code: string, lang: string, want: string[][]) {
    const got = await tokens(page, code, lang);
    check(`highlight ${lang}: ${JSON.stringify(code)}`, JSON.stringify(got) === JSON.stringify(want), { want, got });
}

// the [src] and [glsl] buttons open the source view
async function testSourceView(page: Page, toc: Toc, s: Sample, github: string) {
    const visible = (id: string) => page.$eval(`#${id}`, (e) => !(e as HTMLElement).hidden);
    const active = (id: string) => page.$eval(`#${id}`, (e) => e.classList.contains('active'));
    const loaded = () => page.waitForFunction(() => document.getElementById('code-text')?.textContent !== 'loading...');
    const codeText = () => page.$eval('#code-text', (e) => e.textContent ?? '');

    const numHighlightScripts = () => page.$$eval('script[src="highlight.js"]', (els) => els.length);
    check('source view closed', !(await visible('code')));
    check('highlight.js not loaded before source view opens', (await numHighlightScripts()) === 0);
    await page.click('#src-btn');
    await loaded();
    const src = await (await fetch(`${githubRaw}/${s.source}`)).text();
    check('[src] opens source view', await visible('code'));
    check('[src] active', (await active('src-btn')) && !(await active('glsl-btn')));
    check('[src] aria-expanded', (await page.$eval('#src-btn', (e) => e.getAttribute('aria-expanded'))) === 'true');
    check('source view file name', (await page.$eval('#code-file', (e) => e.textContent)) === s.source);
    check('source view github link', (await page.$eval('#code-github', (e) => e.getAttribute('href'))) === `${github}/${s.source}`);
    check('source view text', (await codeText()) === src);
    const numLines = src.split('\n').length - (src.endsWith('\n') ? 1 : 0);
    const lines = await page.$eval('#code-lines', (e) => (e.textContent ?? '').split('\n'));
    check('source view line numbers', (lines.length === numLines) && (lines.at(-1) === String(numLines)), lines.length);
    check('source view highlights C', (await page.$$('#code-text .hl-kw')).length > 0);
    check('source view has focus', (await page.evaluate(() => document.activeElement?.id)) === 'code-scroll');
    const codeScrollbar = await page.$eval('#code-scroll', (e) => (e as HTMLElement).offsetWidth - e.clientWidth);
    check('source view themed scrollbar', codeScrollbar === 12, codeScrollbar);

    // [glsl] switches to the shader, keeps the scroll position of each file
    await page.$eval('#code-scroll', (e) => e.scrollTop = 100);
    await page.click('#glsl-btn');
    await loaded();
    const shader = await (await fetch(`${githubRaw}/${s.shader}`)).text();
    check('[glsl] shows shader', (await codeText()) === shader);
    check('[glsl] active', (await active('glsl-btn')) && !(await active('src-btn')));
    check('[glsl] highlights shdc tags', (await page.$$eval('#code-text .hl-tag', (els) => els.map((e) => e.textContent))).includes('@end'));
    await page.click('#src-btn');
    await loaded();
    check('source view keeps scroll position', (await page.$eval('#code-scroll', (e) => e.scrollTop)) === 100);

    // clicking the active button closes the view, also [x] and Escape
    await page.click('#src-btn');
    check('[src] again closes source view', !(await visible('code')));
    check('[src] not active after close', !(await active('src-btn')));
    await page.click('#glsl-btn');
    await loaded();
    await page.click('#code-close');
    check('[x] closes source view', !(await visible('code')));
    await page.click('#src-btn');
    await loaded();
    await page.keyboard.press('Escape');
    check('Escape closes source view', !(await visible('code')));
    check('highlight.js loaded once', (await numHighlightScripts()) === 1, await numHighlightScripts());

    // drag the left border to resize the source view (no slide-in animation)
    await page.emulateMediaFeatures([{ name: 'prefers-reduced-motion', value: 'reduce' }]);
    const panelWidth = () => page.$eval('#code', (e) => (e as HTMLElement).offsetWidth);
    const storedWidth = () => page.evaluate(() => localStorage.getItem('code-width'));
    const stageWidth = await page.$eval('.stage', (e) => e.clientWidth);
    const drag = async (dx: number) => {
        const box = (await (await page.$('#code-resize'))!.boundingBox())!;
        const x = box.x + box.width / 2;
        const y = box.y + box.height / 2;
        await page.mouse.move(x, y);
        await page.mouse.down();
        await page.mouse.move(x + dx / 2, y);
        await page.mouse.move(x + dx, y);
        await page.mouse.up();
    };
    await page.click('#src-btn');
    await loaded();
    const w0 = await panelWidth();
    await drag(100);
    check('drag resizes source view', (await panelWidth()) === w0 - 100, { w0, got: await panelWidth() });
    check('drag stores width', (await storedWidth()) === String(w0 - 100), await storedWidth());
    check('drag ends resizing', !(await page.$eval('body', (e) => e.classList.contains('resizing'))));
    await drag(-10000);
    check('drag clamps to stage width', (await panelWidth()) === stageWidth, await panelWidth());
    await drag(10000);
    check('drag clamps to min width', (await panelWidth()) === 320, await panelWidth());
    await page.reload({ waitUntil: 'networkidle0' });
    await page.click('#src-btn');
    await loaded();
    check('width persists after reload', (await panelWidth()) === 320, await panelWidth());
    await page.click('#code-resize', { clickCount: 2 });
    check('double-click resets width', ((await panelWidth()) === w0) && ((await storedWidth()) === null), await panelWidth());
    await page.click('#code-resize');
    check('click without drag keeps default width', (await storedWidth()) === null, await storedWidth());
    await page.focus('#code-resize');
    await page.keyboard.press('ArrowRight');
    check('ArrowRight shrinks source view', (await panelWidth()) === w0 - 32, await panelWidth());
    await page.keyboard.press('ArrowLeft');
    check('ArrowLeft grows source view', (await panelWidth()) === w0, await panelWidth());
    await page.evaluate(() => localStorage.removeItem('code-width'));
    await page.keyboard.press('Escape');
    await page.emulateMediaFeatures([]);

    // the highlighter
    await checkTokens(page, '#include <stdio.h>\nint x = 0x1F; // c', 'c',
        [['pre', '#include'], ['str', '<stdio.h>'], ['type', 'int'], ['num', '0x1F'], ['com', '// c']]);
    await checkTokens(page, 'static float f(void) { return 1.5f * SG_MAX; }', 'c',
        [['kw', 'static'], ['type', 'float'], ['fn', 'f'], ['type', 'void'], ['kw', 'return'], ['num', '1.5f'], ['const', 'SG_MAX']]);
    await checkTokens(page, '/* a\n"b" */ char* s = "x\\"y"; uint32_t c = \'\\n\';', 'c',
        [['com', '/* a\n"b" */'], ['type', 'char'], ['str', '"x\\"y"'], ['type', 'uint32_t'], ['str', "'\\n'"]]);
    await checkTokens(page, 'class A { A* p = nullptr; };', 'cpp', [['kw', 'class'], ['kw', 'nullptr']]);
    await checkTokens(page, 'class A {};', 'c', []);
    await checkTokens(page, '@vs vs\nuniform sampler2D tex;\nvoid main() { gl_Position = vec4(0.5); }\n@end', 'glsl',
        [['tag', '@vs'], ['kw', 'uniform'], ['type', 'sampler2D'], ['type', 'void'], ['fn', 'main'], ['var', 'gl_Position'], ['type', 'vec4'], ['num', '0.5'], ['tag', '@end']]);
    await checkTokens(page, '@ctype mat4 HMM_Mat4\n  @end x @y', 'glsl', [['tag', '@ctype'], ['type', 'mat4'], ['tag', '@end']]);
    await checkTokens(page, '  # if FOO\nx # y', 'c', [['pre', '# if'], ['const', 'FOO']]);
    await checkTokens(page, '@vs vs', 'c', []);
    await checkTokens(page, '/* unterminated\nint x;', 'c', [['com', '/* unterminated\nint x;']]);

    // the highlighter keeps the text of all local source files
    const files = toc.samples.flatMap((s) => s.shader ? [s.source, s.shader] : [s.source])
        .map((file) => [file, Deno.readTextFileSync(`${rootDir}/sapp/${file}`)]);
    const changed = await page.evaluate((files) => {
        const res = [];
        for (const [file, text] of files) {
            // @ts-ignore: highlight() and langOf() are in highlight.js
            const frag: DocumentFragment = highlight(text, langOf(file));
            if (frag.textContent !== text) {
                res.push(file);
            }
        }
        return res;
    }, files);
    check(`highlight keeps the text of ${files.length} files`, changed.length === 0, changed);
}

async function testPlayer(url: string) {
    console.log(`testing ${url}player.html`);
    const browser = await launch();
    try {
        const page = await browser.newPage();
        const errors: string[] = [];
        page.on('pageerror', (e) => errors.push(String(e)));
        const toc: Toc = await (await fetch(new URL('samples.json', url))).json();
        const other = (toc.api === 'webgl') ? 'webgpu' : 'webgl';
        const visible = (id: string) => page.$eval(`#${id}`, (e) => !(e as HTMLElement).hidden);
        const href = (id: string) => page.$eval(`#${id}`, (e) => e.getAttribute('href'));
        const github = 'https://github.com/floooh/sokol-samples/tree/master/sapp';

        // a sample with shader and debug UI
        const s = toc.samples.find((s) => s[toc.api] && s.ui && s.shader)!;
        await page.goto(`${url}player.html?name=${s.name}`, { waitUntil: 'networkidle0' });
        check('player title', (await page.$eval('#title', (e) => e.textContent)) === s.name);
        check('player iframe src', (await page.$eval('#frame', (e) => e.getAttribute('src'))) === `${s.name}-sapp.html`);
        check('[ui] link', (await href('ui-link')) === `player.html?name=${s.name}&ui=1`);
        check('[new tab] link', (await href('open-link')) === `${s.name}-sapp.html`);
        const crossUrl = (toc.api === 'webgl') ? 'https://floooh.github.io/sokol-webgpu' : 'https://floooh.github.io/sokol-html5';
        const otherTag = (other === 'webgl') ? 'webgl2' : 'webgpu';
        if (s[other]) {
            check('cross api link', (await href('cross-api-link')) === `${crossUrl}/player.html?name=${s.name}`, await href('cross-api-link'));
            check('cross api link text', (await page.$eval('#cross-api-link', (e) => e.textContent)) === `[-> #${otherTag}]`);
        }
        check('[fullscreen] shown', await visible('fs-btn'));
        // the sample runs, if it is built (see preview.ts --dist)
        if ((await fetch(new URL(`${s.name}-sapp.html`, url))).ok) {
            const frame = await (await page.$('#frame'))!.contentFrame();
            check('sample runs in iframe', (frame !== null) && ((await frame.$('canvas#canvas')) !== null));
            check('iframe has focus', (await page.evaluate(() => document.activeElement?.id)) === 'frame');
            // [fullscreen] puts the canvas in the iframe in fullscreen (not the iframe itself)
            await page.click('#fs-btn');
            await page.waitForFunction(() => document.fullscreenElement !== null, { timeout: 2000 }).catch(() => {});
            check('[fullscreen] iframe in fullscreen', (await page.evaluate(() => document.fullscreenElement?.id)) === 'frame');
            check('[fullscreen] canvas in fullscreen', (await frame!.evaluate(() => document.fullscreenElement?.id)) === 'canvas');
            await frame!.evaluate(() => document.exitFullscreen());
            await page.waitForFunction(() => document.fullscreenElement === null, { timeout: 2000 }).catch(() => {});
        }

        await testSourceView(page, toc, s, github);

        // [desc] toggles the description panel, Escape closes it
        check('desc closed', !(await visible('desc')));
        await page.click('#desc-btn');
        check('[desc] opens', await visible('desc'));
        check('desc text', (await page.$eval('#desc-text', (e) => e.textContent)) === s.description);
        const descTags = await page.$$eval('#desc-list dd:first-of-type a', (els) => els.map((e) => e.textContent));
        check('desc tags', sameList(descTags as string[], s.tags), descTags);
        await page.click('#desc-btn');
        check('[desc] closes', !(await visible('desc')));
        await page.click('#desc-btn');
        await page.focus('#desc-btn');
        await page.keyboard.press('Escape');
        check('Escape closes desc', !(await visible('desc')));

        // the file links in the desc panel open the source view
        await page.click('#desc-btn');
        await page.click(`#desc-list a[href="${github}/${s.shader}"]`);
        await page.waitForFunction(() => document.getElementById('code-text')?.textContent !== 'loading...');
        check('desc file link closes desc', !(await visible('desc')));
        check('desc file link opens source view', await visible('code'));
        check('desc file link shows file', (await page.$eval('#code-file', (e) => e.textContent)) === s.shader);
        check('desc file link sets [glsl] active', await page.$eval('#glsl-btn', (e) => e.classList.contains('active')));
        await page.keyboard.press('Escape');

        // the debug UI variant
        await page.goto(`${url}player.html?name=${s.name}&ui=1`, { waitUntil: 'networkidle0' });
        check('ui iframe src', (await page.$eval('#frame', (e) => e.getAttribute('src'))) === `${s.name}-sapp-ui.html`);
        check('[ui] active', await page.$eval('#ui-link', (e) => e.classList.contains('active')));
        check('[ui] link back', (await href('ui-link')) === `player.html?name=${s.name}`);
        if (s[other]) {
            check('cross api link keeps ui', (await href('cross-api-link')) === `${crossUrl}/player.html?name=${s.name}&ui=1`, await href('cross-api-link'));
        }

        // a sample without shader and debug UI
        const t = toc.samples.find((s) => s[toc.api] && !s.ui && !s.shader);
        if (t) {
            await page.goto(`${url}player.html?name=${t.name}&ui=1`, { waitUntil: 'networkidle0' });
            check('no [glsl] without shader', !(await visible('glsl-btn')));
            check('no [ui] without debug UI', !(await visible('ui-link')));
            check('ui=1 ignored without debug UI', (await page.$eval('#frame', (e) => e.getAttribute('src'))) === `${t.name}-sapp.html`);
        }

        // a sample which only runs on this api has no cross api link
        const v = toc.samples.find((s) => s[toc.api] && !s[other]);
        if (v) {
            await page.goto(`${url}player.html?name=${v.name}`, { waitUntil: 'networkidle0' });
            check('no cross api link for single-api sample', !(await visible('cross-api-link')));
        }

        // a sample which does not run on this api
        const u = toc.samples.find((s) => !s[toc.api] && s[other]);
        if (u) {
            await page.goto(`${url}player.html?name=${u.name}`, { waitUntil: 'networkidle0' });
            check('unsupported: no iframe', !(await visible('frame')));
            const link = await page.$eval('#message a', (e) => e.getAttribute('href'));
            check('unsupported: links to other page', link?.endsWith(`/player.html?name=${u.name}`) ?? false, link);
        }

        // unknown sample
        await page.goto(`${url}player.html?name=xyzzy`, { waitUntil: 'networkidle0' });
        check('unknown sample message', (await page.$eval('#message', (e) => e.textContent ?? '')).includes('no such sample'));
        check('unknown sample: no iframe', !(await visible('frame')));

        // the player has no theme toggle and uses the theme of the gallery
        const theme = () => page.evaluate(() => document.documentElement.dataset.theme);
        check('player: no theme button', (await page.$('#theme-btn')) === null);
        check('player: default theme is dark', (await theme()) === 'dark');
        await page.evaluate(() => localStorage.setItem('theme', 'light'));
        await page.reload({ waitUntil: 'networkidle0' });
        check('player: uses gallery theme', (await theme()) === 'light');
        await page.evaluate(() => localStorage.removeItem('theme'));

        // back goes to the gallery and keeps the search
        await page.goto(`${url}?q=cube`, { waitUntil: 'networkidle0' });
        await Promise.all([page.waitForNavigation(), page.click('.card:not([hidden]) .card-title a')]);
        await Promise.all([page.waitForNavigation(), page.click('#back')]);
        check('back keeps search', new URL(page.url()).searchParams.get('q') === 'cube', page.url());

        check('player: no page errors', errors.length === 0, errors);
    } finally {
        await browser.close();
    }
}

for (const url of urls) {
    await testPage(url);
    await testPlayer(url);
}
console.log(numFailed === 0 ? 'all tests passed' : `${numFailed} tests FAILED`);
Deno.exit(numFailed === 0 ? 0 : 1);
