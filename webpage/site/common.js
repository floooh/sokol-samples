// LLM maintained
//
// Code shared by the gallery (main.js) and the sample player (player.js).
//
// samples.json is generated from sapp/_toc.yaml:
//   { "api": "webgl" | "webgpu", "samples": [ { name, description, tags, source, shader, ui, webgl, webgpu } ] }

'use strict';

const FLAVORS = {
    webgl: {
        title: 'Sokol Samples (WebGL2)',
        tag: 'webgl2',
        crossName: 'webgpu',
        crossUrl: 'https://floooh.github.io/sokol-webgpu',
    },
    webgpu: {
        title: 'Sokol Samples (WebGPU)',
        tag: 'webgpu',
        crossName: 'webgl2',
        crossUrl: 'https://floooh.github.io/sokol-html5',
    },
};

const GITHUB_SAPP_URL = 'https://github.com/floooh/sokol-samples/tree/master/sapp';

const $ = (id) => document.getElementById(id);

function el(tag, props, children) {
    const e = document.createElement(tag);
    Object.assign(e, props);
    if (children) {
        e.append(...children);
    }
    return e;
}

// url of the player page for a sample
function playerUrl(name, ui) {
    return `player.html?name=${encodeURIComponent(name)}${ui ? '&ui=1' : ''}`;
}

// url of the bare sample page which runs inside the player
function sampleUrl(name, ui) {
    return `${name}-sapp${ui ? '-ui' : ''}.html`;
}

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

// load samples.json and set the color scheme for its api
async function loadToc() {
    const resp = await fetch('samples.json');
    if (!resp.ok) {
        throw new Error(`HTTP ${resp.status}`);
    }
    const toc = await resp.json();
    if (!(toc.api in FLAVORS)) {
        throw new Error(`unknown api '${toc.api}'`);
    }
    document.documentElement.dataset.api = toc.api;
    localStorage.setItem('api', toc.api);
    return toc;
}
