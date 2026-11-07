// LLM maintained
//
// Generate the pixel-art SOKOL logo as SVG.
//
// Usage (from the project root):
//
//   deno run -A webpage/tools/logo.ts
//
// Replaces the inline <svg class="logo"> in webpage/site/index.html and
// writes webpage/site/favicon.svg (without the glow). Change the font
// grid, colors and sizes below, then run the script and set the .logo width
// in style.css to the printed logo width (keep 1:1 so that the edges stay
// crisp).

// font pixels, one color per row
const grid = [
    '.####....###...###..##...###...####...',
    '##..##..##.##...##..##..##.##...##....',
    '##.....##...##..##.##..##...##..##....',
    '.####..##...##..####...##...##..##....',
    '....##.##...##..##.##..##...##..##...#',
    '##..##..##.##...##..##..##.##...##..##',
    '.####....###...###..##...###...#######',
];
const colors = ['#ff7043', '#ffa726', '#ffee58', '#d4e157', '#9ccc65', '#66bb6a', '#42a5f5'];
const C = 11;   // size of one font pixel
const B = 1;    // outline width
const OUTLINE = '#111416';
const rect = (x: number, y: number, w: number, h: number) => `M${x} ${y}h${w}v${h}h${-w}z`;

// merge horizontal runs of font pixels in one row into single rects,
// grow each rect by 'grow' units on all sides
function runs(cells: [number, number][], y: number, grow: number): string {
    const xs = cells.filter((c) => c[1] === y).map((c) => c[0]).sort((a, b) => a - b);
    let d = '';
    for (let i = 0; i < xs.length;) {
        let j = i;
        while ((j + 1 < xs.length) && (xs[j + 1] === xs[j] + 1)) {
            j += 1;
        }
        d += rect(B + xs[i] * C - grow, B + y * C - grow, (j - i + 1) * C + 2 * grow, C + 2 * grow);
        i = j + 1;
    }
    return d;
}

function svg(cells: [number, number][], cols: number, attrs: string, indent: string, glow: boolean): string {
    const W = cols * C + 2 * B, HH = grid.length * C + 2 * B;
    const outline = `${indent}    <path style="fill:${OUTLINE}" d="${grid.map((_, y) => runs(cells, y, B)).join('')}"/>`;
    const rows = colors.map((col, y) => [col, runs(cells, y, 0)])
        .filter(([, d]) => d !== '')
        .map(([col, d]) => `${indent}    <path style="fill:${col}" d="${d}"/>`);
    const open = `${indent}<svg ${attrs}xmlns="http://www.w3.org/2000/svg" viewBox="0 0 ${W} ${HH}" width="${W}" height="${HH}" shape-rendering="crispEdges" role="img" aria-label="SOKOL">`;
    if (!glow) {
        return [open, outline, ...rows, `${indent}</svg>`].join('\n');
    }
    // soft phosphor glow
    const i = `${indent}    `;
    return [
        open,
        `${i}<defs>`,
        `${i}    <filter id="logo-glow" x="-5%" y="-20%" width="110%" height="140%">`,
        `${i}        <feGaussianBlur in="SourceGraphic" stdDeviation="2"/>`,
        `${i}        <feComponentTransfer><feFuncA type="linear" slope="0.5"/></feComponentTransfer>`,
        `${i}        <feMerge><feMergeNode/><feMergeNode in="SourceGraphic"/></feMerge>`,
        `${i}    </filter>`,
        `${i}</defs>`,
        outline,
        `${i}<g filter="url(#logo-glow)">`,
        ...rows.map((l) => `    ${l}`),
        `${i}</g>`,
        `${indent}</svg>`,
    ].join('\n');
}

const cells: [number, number][] = [];
grid.forEach((row, y) => [...row].forEach((ch, x) => { if (ch === '#') cells.push([x, y]); }));
const cols = Math.max(...grid.map((r) => r.lastIndexOf('#') + 1));

// inline logo in index.html
const siteDir = new URL('../site', import.meta.url).pathname;
const htmlPath = `${siteDir}/index.html`;
const html = Deno.readTextFileSync(htmlPath);
const m = html.match(/( *)<svg class="logo"[\s\S]*?<\/svg>/);
if (!m) throw new Error('logo svg not found');
Deno.writeTextFileSync(htmlPath, html.replace(m[0], svg(cells, cols, 'class="logo" ', m[1], true)));

// favicon: the 'S' only, in a square viewBox
const favPath = `${siteDir}/favicon.svg`;
let fav = svg(cells.filter(([x]) => x < 6), 6, '', '', false);
const fw = 6 * C + 2 * B, fh = grid.length * C + 2 * B;
fav = fav.replace(`viewBox="0 0 ${fw} ${fh}" width="${fw}" height="${fh}"`, `viewBox="${-(fh - fw) / 2} 0 ${fh} ${fh}"`)
    .replace(' role="img" aria-label="SOKOL"', '');
Deno.writeTextFileSync(favPath, `<!-- LLM maintained -->\n${fav}\n`);
console.log(`logo ${cols * C + 2 * B}x${fh}`);
