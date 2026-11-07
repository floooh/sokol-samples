// LLM maintained
//
// Build the samples gallery from samples.json, see common.js.

'use strict';

let cards = [];

// theme

function updateThemeButton() {
    const theme = document.documentElement.dataset.theme;
    $('theme-btn').textContent = (theme === 'dark') ? '[light]' : '[dark]';
}

function setupTheme() {
    $('theme-btn').onclick = () => {
        const theme = (document.documentElement.dataset.theme === 'dark') ? 'light' : 'dark';
        document.documentElement.dataset.theme = theme;
        localStorage.setItem('theme', theme);
        updateThemeButton();
    };
    updateThemeButton();
}

// gallery

function makeCard(sample) {
    const url = playerUrl(sample.name, false);
    const img = el('img', {
        className: 'card-image',
        src: `${sample.name}.webp`,
        alt: '',
        width: 512,
        height: 384,
        loading: 'lazy',
    });
    img.onerror = () => {
        img.onerror = null;
        img.src = 'dummy.webp';
    };
    const tags = sample.tags.map((tag) => {
        const btn = el('button', { className: 'tag', type: 'button', textContent: tag });
        btn.onclick = () => setQuery(`#${tag}`);
        return el('li', {}, [btn]);
    });
    const bar = el('div', { className: 'card-bar' }, [
        el('h2', { className: 'card-title' }, [el('a', { href: url, textContent: sample.name })]),
    ]);
    if (sample.ui) {
        bar.append(el('a', {
            className: 'ui-btn',
            href: playerUrl(sample.name, true),
            title: 'Run with debug UI',
            textContent: 'UI',
        }));
    }
    const card = el('article', { className: 'card' }, [
        bar,
        el('a', { className: 'card-shot', href: url, tabIndex: -1, title: sample.description }, [img]),
        el('p', { className: 'card-desc', textContent: sample.description }),
        el('ul', { className: 'card-tags' }, tags),
    ]);
    return { sample, card, tagButtons: tags.map((li) => li.firstChild) };
}

function parseQuery(query) {
    return query.toLowerCase().split(/[\s,]+/).filter((t) => t.replace(/^#/, '').length > 0);
}

// a term with a leading '#' only matches tags, other terms match name or tags
function matchTerm(sample, term) {
    if (term.startsWith('#')) {
        const t = term.slice(1);
        return sample.tags.some((tag) => tag.includes(t));
    }
    return sample.name.includes(term) || sample.tags.some((tag) => tag.includes(term));
}

// each search term must match the sample
function filter(query) {
    const terms = parseQuery(query);
    let numVisible = 0;
    for (const { sample, card, tagButtons } of cards) {
        const visible = terms.every((t) => matchTerm(sample, t));
        card.hidden = !visible;
        if (visible) {
            numVisible += 1;
        }
        for (const btn of tagButtons) {
            const tag = btn.textContent;
            btn.classList.toggle('match', terms.some((t) => tag.includes(t.replace(/^#/, ''))));
        }
    }
    $('count').textContent = `[${numVisible}/${cards.length}]`;
    $('search-clear').hidden = query.length === 0;
    const msg = $('message');
    msg.hidden = numVisible > 0;
    msg.textContent = `find: '${query.trim()}': no such sample`;
    const url = new URL(location.href);
    if (terms.length > 0) {
        url.searchParams.set('q', query.trim());
    } else {
        url.searchParams.delete('q');
    }
    history.replaceState(null, '', url);
}

function setQuery(query) {
    $('search').value = query;
    filter(query);
    $('search').scrollIntoView({ block: 'nearest' });
}

function setupSearch() {
    const search = $('search');
    search.oninput = () => filter(search.value);
    $('search-clear').onclick = () => {
        setQuery('');
        search.focus();
    };
    search.onkeydown = (ev) => {
        if (ev.key === 'Enter') {
            // run the first matching sample
            const first = cards.find((c) => !c.card.hidden);
            if (first) {
                location.href = playerUrl(first.sample.name, false);
            }
        }
    };
    document.addEventListener('keydown', (ev) => {
        if ((ev.key === '/') && (document.activeElement !== search)) {
            // press '/' to focus the search field
            ev.preventDefault();
            search.focus();
        } else if (ev.key === 'Escape') {
            // press Escape anywhere to clear the search and focus the search field
            ev.preventDefault();
            setQuery('');
            search.focus();
        }
    });
    search.value = new URL(location.href).searchParams.get('q') ?? '';
    filter(search.value);
}

function setupFlavor(api) {
    const flavor = FLAVORS[api];
    document.title = flavor.title;
    $('api-badge').textContent = flavor.tag;
    $('cross-name').textContent = flavor.crossName;
    $('cross-link').href = flavor.crossUrl;
}

async function main() {
    setupTheme();
    try {
        const toc = await loadToc();
        setupFlavor(toc.api);
        cards = toc.samples.filter((s) => s[toc.api]).map(makeCard);
        $('gallery').append(...cards.map((c) => c.card));
        setupSearch();
    } catch (err) {
        const msg = $('message');
        msg.hidden = false;
        msg.textContent = `ERROR: failed to load samples.json (${err.message})`;
    }
}

main();
