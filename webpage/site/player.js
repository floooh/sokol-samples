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

function setDescOpen(open) {
    $('desc').hidden = !open;
    $('desc-btn').setAttribute('aria-expanded', String(open));
    $('desc-btn').classList.toggle('active', open);
}

function setupButtons() {
    $('desc-btn').onclick = () => setDescOpen($('desc').hidden);
    $('fs-btn').onclick = () => {
        const frame = $('frame');
        const req = frame.requestFullscreen ?? frame.webkitRequestFullscreen;
        if (req) {
            Promise.resolve(req.call(frame)).then(() => frame.focus()).catch(() => {});
        }
    };
    document.addEventListener('keydown', (ev) => {
        if ((ev.key === 'Escape') && !$('desc').hidden) {
            setDescOpen(false);
            $('frame').focus();
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
    setLink('src-link', `${GITHUB_SAPP_URL}/${sample.source}`);
    if (sample.shader) {
        setLink('glsl-link', `${GITHUB_SAPP_URL}/${sample.shader}`);
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
