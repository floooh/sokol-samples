// LLM maintained
//
// Run one sample in an iframe, see player.html.
//
// Url parameters:
//   name: the sample name
//   ui=1: run the sample variant with the debug UI

'use strict';

function setLink(id, href) {
    const a = $(id);
    a.href = href;
    a.hidden = false;
}

function showMessage(children) {
    const msg = $('message');
    msg.replaceChildren(...children);
    msg.hidden = false;
}

function tagLink(tag, href) {
    return el('a', { className: 'tag', href, textContent: tag });
}

function fillDescription(toc, sample) {
    $('desc-text').textContent = sample.description;
    const row = (term, children) => [el('dt', { textContent: term }), el('dd', {}, children)];
    const githubLink = (file) => el('a', { href: `${GITHUB_SAPP_URL}/${file}`, target: '_blank', rel: 'noopener', textContent: file });
    const files = [githubLink(sample.source)];
    if (sample.shader) {
        files.push(' ', githubLink(sample.shader));
    }
    // link each api tag to the player on that api's page
    const apis = Object.entries(FLAVORS).filter(([api]) => sample[api]).map(([api, flavor]) => {
        const base = (api === toc.api) ? '' : `${FLAVORS[toc.api].crossUrl}/`;
        return tagLink(flavor.tag, `${base}${playerUrl(sample.name, false)}`);
    });
    const tags = sample.tags.map((tag) => tagLink(tag, `index.html?q=${encodeURIComponent(`#${tag}`)}`));
    $('desc-list').replaceChildren(
        ...row('tags', tags),
        ...row('runs on', apis),
        ...row('debug ui', [sample.ui ? 'yes' : 'no']),
        ...row('files', files),
    );
}

// the source view: the shown file, the loaded files and their scroll positions
const code = { file: null, texts: new Map(), scroll: new Map() };

function setCodeButtons() {
    for (const btn of [$('src-btn'), $('glsl-btn')]) {
        const active = (code.file !== null) && (btn.dataset.file === code.file);
        btn.classList.toggle('active', active);
        btn.setAttribute('aria-expanded', String(active));
    }
}

// load highlight.js on first use, it is only needed by the source view
let highlighter = null;
function loadHighlighter() {
    highlighter ??= new Promise((resolve, reject) => {
        const script = el('script', { src: 'highlight.js', onload: resolve });
        script.onerror = () => {
            highlighter = null;
            script.remove();
            reject(new Error('failed to load highlight.js'));
        };
        document.head.append(script);
    });
    return highlighter;
}

async function loadSource(file) {
    if (!code.texts.has(file)) {
        const resp = await fetch(`${GITHUB_RAW_SAPP_URL}/${file}`);
        if (!resp.ok) {
            throw new Error(`HTTP ${resp.status}`);
        }
        code.texts.set(file, await resp.text());
    }
    return code.texts.get(file);
}

// show a file in the source view, null closes the view
async function showCode(file) {
    const scroll = $('code-scroll');
    if (code.file !== null) {
        code.scroll.set(code.file, scroll.scrollTop);
    }
    code.file = file;
    setCodeButtons();
    if (file === null) {
        $('code').hidden = true;
        $('frame').focus();
        return;
    }
    $('code-file').textContent = file;
    $('code-github').href = `${GITHUB_SAPP_URL}/${file}`;
    $('code-lines').textContent = '';
    $('code-text').textContent = 'loading...';
    $('code').hidden = false;
    let text;
    try {
        [text] = await Promise.all([loadSource(file), loadHighlighter()]);
    } catch (err) {
        if (code.file === file) {
            $('code-text').textContent = `ERROR: failed to load ${file} (${err.message})`;
        }
        return;
    }
    // another file was selected while loading
    if (code.file !== file) {
        return;
    }
    const numLines = text.split('\n').length - (text.endsWith('\n') ? 1 : 0);
    $('code-lines').textContent = Array.from({ length: numLines }, (_, i) => i + 1).join('\n');
    $('code-text').replaceChildren(highlight(text, langOf(file)));
    scroll.scrollTop = code.scroll.get(file) ?? 0;
    scroll.focus({ preventScroll: true });
}

// the width of the source view in pixels, null is the default width (see style.css)
const CODE_MIN_WIDTH = 320;

function setCodeWidth(width) {
    const panel = $('code');
    if (width === null) {
        panel.style.width = '';
        localStorage.removeItem('code-width');
        return;
    }
    const max = panel.parentElement.clientWidth;
    width = Math.round(Math.min(Math.max(width, CODE_MIN_WIDTH), max));
    panel.style.width = `${width}px`;
}

// only store a width set by setCodeWidth(), a click without drag keeps the default width
function saveCodeWidth() {
    const width = parseInt($('code').style.width);
    if (width > 0) {
        localStorage.setItem('code-width', String(width));
    }
}

// drag the left border of the source view to resize it, the pointer capture
// keeps the events when the pointer is over the sample iframe
function setupCodeResize() {
    const handle = $('code-resize');
    const panel = $('code');
    const width = Number(localStorage.getItem('code-width'));
    if (width > 0) {
        panel.style.width = `${width}px`;
    }
    handle.onpointerdown = (ev) => {
        if (ev.button !== 0) {
            return;
        }
        ev.preventDefault();
        handle.setPointerCapture(ev.pointerId);
        document.body.classList.add('resizing');
    };
    handle.onpointermove = (ev) => {
        if (handle.hasPointerCapture(ev.pointerId)) {
            setCodeWidth(panel.getBoundingClientRect().right - ev.clientX);
        }
    };
    handle.onlostpointercapture = () => {
        document.body.classList.remove('resizing');
        saveCodeWidth();
    };
    handle.ondblclick = () => setCodeWidth(null);
    // the arrow keys resize the focused handle, shift for bigger steps
    handle.onkeydown = (ev) => {
        const step = ev.shiftKey ? 128 : 32;
        if (ev.key === 'ArrowLeft') {
            setCodeWidth(panel.offsetWidth + step);
        } else if (ev.key === 'ArrowRight') {
            setCodeWidth(panel.offsetWidth - step);
        } else {
            return;
        }
        ev.preventDefault();
        saveCodeWidth();
    };
}

function setupCodeButton(id, file) {
    const btn = $(id);
    btn.dataset.file = file;
    btn.onclick = () => showCode((code.file === file) ? null : file);
    btn.hidden = false;
}

function setDescOpen(open) {
    $('desc').hidden = !open;
    $('desc-btn').setAttribute('aria-expanded', String(open));
    $('desc-btn').classList.toggle('active', open);
}

function setupButtons() {
    $('desc-btn').onclick = () => setDescOpen($('desc').hidden);
    $('fs-btn').onclick = () => {
        const frame = $('frame');
        // fullscreen the sample canvas like sapp_toggle_fullscreen(), so that the
        // sample gets the fullscreenchange event (and sapp_is_fullscreen() is correct)
        const el = frame.contentDocument?.getElementById('canvas') ?? frame;
        const req = el.requestFullscreen ?? el.webkitRequestFullscreen;
        if (req) {
            Promise.resolve(req.call(el)).then(() => frame.focus()).catch(() => {});
        }
    };
    $('code-close').onclick = () => showCode(null);
    setupCodeResize();
    // Escape closes the description first, then the source view
    document.addEventListener('keydown', (ev) => {
        if (ev.key !== 'Escape') {
            return;
        }
        if (!$('desc').hidden) {
            setDescOpen(false);
            $('frame').focus();
        } else if (code.file !== null) {
            showCode(null);
        }
    });
    // go back to the gallery with its search and scroll position, if we came from there
    $('back').onclick = (ev) => {
        const ref = document.referrer ? new URL(document.referrer) : null;
        if (ref && (ref.origin === location.origin) && /\/(index\.html)?$/.test(ref.pathname)) {
            ev.preventDefault();
            history.back();
        }
    };
}

async function main() {
    setupButtons();
    const params = new URL(location.href).searchParams;
    const name = params.get('name') ?? '';
    let toc;
    try {
        toc = await loadToc();
    } catch (err) {
        showMessage([`ERROR: failed to load samples.json (${err.message})`]);
        return;
    }
    const flavor = FLAVORS[toc.api];
    $('api-badge').textContent = flavor.tag;
    $('title').textContent = name;

    const sample = toc.samples.find((s) => s.name === name);
    if (!sample) {
        document.title = `${flavor.title}`;
        showMessage([`no such sample: '${name}' `, el('a', { href: 'index.html', textContent: '[<- samples]' })]);
        return;
    }
    const ui = sample.ui && (params.get('ui') === '1');
    document.title = `${name}${ui ? ' (ui)' : ''} - ${flavor.title}`;
    setupCodeButton('src-btn', sample.source);
    if (sample.shader) {
        setupCodeButton('glsl-btn', sample.shader);
    }
    fillDescription(toc, sample);
    // link to the same sample on the other api's page
    const other = Object.keys(FLAVORS).find((api) => api !== toc.api);
    if (sample[other]) {
        setLink('cross-api-link', `${flavor.crossUrl}/${playerUrl(name, ui)}`);
        $('cross-api-link').textContent = `[-> #${FLAVORS[other].tag}]`;
    }
    $('desc-btn').hidden = false;
    if (sample.ui) {
        setLink('ui-link', playerUrl(name, !ui));
        $('ui-link').classList.toggle('active', ui);
        $('ui-link').setAttribute('aria-pressed', String(ui));
    }

    if (!sample[toc.api]) {
        // the sample does not run on this page's api, link to the other page
        const url = `${flavor.crossUrl}/${playerUrl(name, ui)}`;
        showMessage([`'${name}' does not run on ${flavor.tag}, `, el('a', { href: url, textContent: `[-> #${FLAVORS[other].tag}]` })]);
        return;
    }
    const url = sampleUrl(name, ui);
    setLink('open-link', url);
    $('fs-btn').hidden = false;
    const frame = $('frame');
    frame.title = name;
    frame.addEventListener('load', () => frame.focus());
    frame.src = url;
    frame.hidden = false;
}

main();
