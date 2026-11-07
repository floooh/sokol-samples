## LLM usage rules

LLM generation is strictly **forbidden** in the sample code, shaders and any
human facing documentation (the 'no-LLM zone'):

- ./sapp/ (all files except ./sapp/_toc.yaml)
- ./d3d11/, ./metal/, ./glfw/, ./html5/
- ./libs/ (sample helper code and vendored third-party code)
- README.md

Passive bug scanning, code analysis, code reviewing via LLM is explicitly allowed
and encouraged, but never apply fixes on your own. Present findings and code
snippets in chat instead.

LLM generation is allowed for:

- maintaining the build scripts in `./fibs.ts` and `./fibs-scripts/`
- maintaining the CI workflows in `./.github/workflows/`
- maintaining the samples webpage in `./webpage/` and `./fibs-scripts/webpage.ts`,
  see `./webpage/AGENTS.md`
- maintaining the sample metadata in `./sapp/_toc.yaml`

Keep comments short, concise, to-the-point and use 'Simplified Technical English'.

When creating files, add a 'LLM maintained' note at the top.

When editing files without the 'LLM maintained' note, ask the user whether
the note should be added.

Never git add, commit or push on your own.

## Sokol headers

The sokol headers are imported from https://github.com/floooh/sokol into
`./.fibs/imports/sokol/`. A link in `./.fibs/links.json` (see `./fibs link`)
overrides the import, then use the headers in the linked directory.

The rules in the sokol `AGENTS.md` apply to the headers. Never edit files
in `./.fibs/imports/`.

## Reviewing Code

When reviewing code, assume debug build mode (asserts enabled, unreachable panics,
validation layers enabled).
