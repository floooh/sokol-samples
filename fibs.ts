import { type Builder, type Configurer } from 'jsr:@floooh/fibs@^1';
import { addImports } from './fibs-scripts/imports.ts';
import { addLibs } from './fibs-scripts/libs.ts';
import { addConfigs } from './fibs-scripts/configs.ts';
import { addSokolAppSamples } from './fibs-scripts/sapp.ts';

export function configure(c: Configurer) {
    addConfigs(c);
    addImports(c);
}

export function build(b: Builder) {
    addLibs(b);
    if (b.activeConfig().name.startsWith('sapp-')) {
        addSokolAppSamples(b);
    }
}
