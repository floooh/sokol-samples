<!-- LLM maintained -->

# Samples webpage

All files in this directory and `../fibs-scripts/webpage.ts` may be LLM maintained.
The rules in `../AGENTS.md` apply.

The webpage has two deployments with the same files, one for WebGL2
(https://floooh.github.io/sokol-html5) and one for WebGPU
(https://floooh.github.io/sokol-webgpu), see `../.github/workflows/deploy.yml`.

## Layout

- `site/`: the page files, deployed as-is (there is no build step)
  - `index.html`, `main.js`: the gallery with the search field and the theme toggle
  - `player.html`, `player.js`: run one sample in an iframe, `[src]` and `[glsl]`
    show the source files in a slide-in source view (loaded from the `master`
    branch on raw.githubusercontent.com, the left border resizes the view)
  - `highlight.js`: minimal syntax highlighter for C, C++ and GLSL (with the
    sokol-shdc `@` tags), `player.js` loads it the first time the source view opens
  - `common.js`: code for both pages (flavors, `samples.json` loading)
  - `style.css`: all styles, colors are CSS variables per theme
    (`data-theme`) and per api (`data-api`, green for WebGL2, amber for WebGPU)
  - `favicon.svg`
- `tools/`: development scripts, not deployed
  - `preview.ts`: serve the page locally
  - `test.ts`: test the page in a headless Chrome
  - `logo.ts`: generate the SOKOL logo
- `screenshots/`: one `[name].jpg` per sample, converted to `.webp` by the build,
  `dummy.jpg` shows for samples without a screenshot
- `shell.html`: currently not used (the build uses the Emscripten shell file
  of fibs-extras)

## Data flow

- `../sapp/_toc.yaml` has one entry per sample (description, tags, source, shader,
  ui, webgl, webgpu) in gallery order, and the list of asset files.
- `./fibs webpage build [webgl|webgpu]` (`../fibs-scripts/webpage.ts`) builds the
  samples and writes the page to `../.fibs/webpage/[api]/`:
  - `samples.json`: `{ api, samples }` with all samples of `_toc.yaml`
  - the files in `site/`
  - the Emscripten output of each sample (`.html`, `.js`, `.wasm`, also the
    `-ui` variant), the `.html` file runs inside the player iframe
  - the asset files and the screenshots as `.webp`
- `webpage.ts` checks `_toc.yaml` against `../fibs-scripts/sapp.ts` (name,
  ui, source file) and stops with an error on a mismatch.

To add a sample to the webpage: add an entry to `_toc.yaml` and a screenshot
(about 512x384) to `screenshots/`. Add the tag `compute` and set `webgl: false`
when the sample needs compute support.

## Rules

- No build step, no npm dependencies, no frameworks, no web fonts.
- Plain JavaScript in classic `defer` scripts, `common.js` loads first.
  Exception: `player.js` loads `highlight.js` on demand.
- Do not edit the logo SVG in `site/index.html` and `site/favicon.svg` by hand,
  change `tools/logo.ts` and run it.
- Hide elements with the `hidden` attribute (`style.css` forces `display: none`).
- Keep `crossorigin` on the `samples.json` preload, it must match `fetch()`,
  else the file loads twice.

## Workflow

- Preview (page files are served live from `site/`, restart after a change
  to `_toc.yaml`):

  ```
  deno run -A webpage/tools/preview.ts webgl --port=8000 --dist=.fibs/dist/[emscripten-config]
  deno run -A webpage/tools/preview.ts webgpu --port=8001
  ```

  `--dist` serves the built samples, so that the player can run them.
  Stop the preview servers you started when you are done. Do not start them
  with `nohup`, else they keep running after the session ends.
- Test (with the preview running), all tests must pass, add tests for new features:

  ```
  deno run -A webpage/tools/test.ts
  ```

- Type check:

  ```
  deno check fibs-scripts/webpage.ts webpage/tools/*.ts
  ```

- Check behavior by driving Chrome (see `tools/test.ts`), use screenshots
  only to check the visual design.
