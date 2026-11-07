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

const urls = Deno.args.length > 0 ? Deno.args : ['http://localhost:8000/', 'http://localhost:8001/'];
const chrome = Deno.env.get('CHROME') ?? '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome';

let numFailed = 0;

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
    const browser = await puppeteer.launch({ executablePath: chrome, headless: true });
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

        // clicking the logo reloads the page without the query
        await Promise.all([page.waitForNavigation({ waitUntil: 'networkidle0' }), page.click('.logo-link')]);
        check('logo click clears url query', new URL(page.url()).searchParams.get('q') === null, page.url());
        check('logo click clears search', (await page.$eval('#search', (e) => (e as HTMLInputElement).value)) === '');
        check('logo click shows all samples', (await visibleCards(page)).length === samples.length);

        // theme toggle, persists after reload
        const theme = () => page.evaluate(() => document.documentElement.dataset.theme);
        const before = await theme();
        check('default theme is dark', before === 'dark', before);
        await page.click('#theme-btn');
        const after = await theme();
        check('theme toggles', (before !== after) && ((after === 'dark') || (after === 'light')), { before, after });
        await page.reload({ waitUntil: 'networkidle0' });
        check('theme persists after reload', (await theme()) === after);

        check('no console errors', errors.length === 0, errors);
    } finally {
        await browser.close();
    }
}

async function testPlayer(url: string) {
    console.log(`testing ${url}player.html`);
    const browser = await puppeteer.launch({ executablePath: chrome, headless: true });
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
        check('[src] link', (await href('src-link')) === `${github}/${s.source}`);
        check('[glsl] link', (await href('glsl-link')) === `${github}/${s.shader}`);
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
        }

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
            check('no [glsl] without shader', !(await visible('glsl-link')));
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
