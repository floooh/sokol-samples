import { type Builder, type Configurer } from 'jsr:@floooh/fibs@^1';
import { addImports } from './fibs-scripts/imports.ts';
import { addLibs } from './fibs-scripts/libs.ts';
import { addConfigs } from './fibs-scripts/configs.ts';
import { addSokolAppSamples } from './fibs-scripts/sapp.ts';
import { addGlfwSamples } from './fibs-scripts/glfw.ts';

export function configure(c: Configurer) {
    addConfigs(c);
    addImports(c);
}

export function build(b: Builder) {
    addLibs(b);
    if (b.activeConfig().options.sapp) {
        addSokolAppSamples(b);
    } else if (b.activeConfig().options.glfw) {
        addGlfwSamples(b);
    }
}
