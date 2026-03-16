import { type Builder, type Configurer } from 'jsr:@floooh/fibs@^1';
import { addImports } from './fibs-scripts/imports.ts';
import { addLibs } from './fibs-scripts/libs.ts';
import { addConfigs } from './fibs-scripts/configs.ts';
import { addSokolAppSamples } from './fibs-scripts/sapp.ts';
import { addGlfwSamples } from './fibs-scripts/glfw.ts';
import { addMetalSamples } from './fibs-scripts/metal.ts';
import { addEmscriptenSamples } from './fibs-scripts/emscripten.ts';

export function configure(c: Configurer) {
    addConfigs(c);
    addImports(c);
}

export function build(b: Builder) {
    addLibs(b);
    const cfg = b.activeConfig();
    if (cfg.options.sappSamples) {
        addSokolAppSamples(b);
    } else if (cfg.options.glfwSamples) {
        addGlfwSamples(b);
    } else if (cfg.options.metalSamples) {
        addMetalSamples(b);
    } else if (cfg.name.startsWith('emsc-')) {
        addEmscriptenSamples(b);
    }
}
