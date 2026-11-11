// LLM maintained
//
// Minimal syntax highlighter for C, C++ and GLSL (with sokol-shdc '@' tags).
//
// highlight(text, lang) returns a DocumentFragment with text nodes and
// <span class="hl-[kind]"> elements, lang is 'c', 'cpp' or 'glsl'.
// Kinds: com, str, num, pre, tag, kw, type, const, fn, var.

'use strict';

const HL_WORDS = (s) => new Set(s.split(/\s+/));

const HL_C_KEYWORDS = HL_WORDS(`
    auto break case const continue default do else enum extern for goto if inline
    register restrict return sizeof static struct switch typedef union volatile while
    _Alignas _Alignof _Atomic _Generic _Noreturn _Static_assert _Thread_local
    alignas alignof static_assert true false NULL nullptr`);

const HL_CPP_KEYWORDS = HL_WORDS(`
    class namespace template typename public private protected virtual override final
    new delete this operator using constexpr consteval constinit noexcept try catch throw
    explicit friend mutable decltype static_cast dynamic_cast reinterpret_cast const_cast`);

const HL_C_TYPES = HL_WORDS(`
    void char short int long float double signed unsigned bool _Bool`);

const HL_GLSL_KEYWORDS = HL_WORDS(`
    attribute const uniform varying buffer shared layout centroid flat smooth
    noperspective patch sample break continue do for while switch case default if else
    in out inout return discard struct precision highp mediump lowp invariant precise
    coherent volatile restrict readonly writeonly true false`);

const HL_GLSL_TYPE = /^(?:void|bool|u?int|float|double|[iudb]?vec[234]|d?mat[234](?:x[234])?|[iu]?(?:sampler|texture|image)[0-9A-Z]\w*|sampler(?:Shadow)?)$/;

const HL_LANGS = {
    c: {
        keyword: (w) => HL_C_KEYWORDS.has(w),
        type: (w) => HL_C_TYPES.has(w) || /_t$/.test(w),
    },
    cpp: {
        keyword: (w) => HL_C_KEYWORDS.has(w) || HL_CPP_KEYWORDS.has(w),
        type: (w) => HL_C_TYPES.has(w) || /_t$/.test(w),
    },
    glsl: {
        keyword: (w) => HL_GLSL_KEYWORDS.has(w),
        type: (w) => HL_GLSL_TYPE.test(w),
        // sokol-shdc tags (@vs, @end, @program, ...) at the start of a line
        tags: true,
    },
};

// one regex for all token kinds, the text between matches is plain text
const HL_TOKEN = new RegExp([
    /(\/\/[^\n]*|\/\*[\s\S]*?(?:\*\/|(?![\s\S])))/.source,    // 1: comment
    /("(?:[^"\\\n]|\\.)*"?|'(?:[^'\\\n]|\\.)*'?)/.source,     // 2: string, char
    /(?<=^[ \t]*)(#[ \t]*\w*)([ \t]*<[^>\n]*>)?/.source,      // 3: preprocessor, 4: <file>
    /(?<=^[ \t]*)(@\w+)/.source,                              // 5: shdc tag
    /((?:\b0[xX][\da-fA-F']+|(?:\b\d[\d']*\.?\d*|\.\d+)(?:[eE][+-]?\d+)?)[uUlLfF]*)/.source, // 6: number
    /([A-Za-z_]\w*)(?=(\s*\()?)/.source,                      // 7: identifier, 8: call
].join('|'), 'gm');

function highlight(text, lang) {
    const rules = HL_LANGS[lang] ?? HL_LANGS.c;
    const frag = document.createDocumentFragment();
    let plain = '';
    const add = (kind, str) => {
        if (!kind) {
            plain += str;
            return;
        }
        if (plain) {
            frag.append(plain);
            plain = '';
        }
        const span = document.createElement('span');
        span.className = `hl-${kind}`;
        span.textContent = str;
        frag.append(span);
    };
    let pos = 0;
    for (const m of text.matchAll(HL_TOKEN)) {
        add(null, text.slice(pos, m.index));
        pos = m.index + m[0].length;
        if (m[1]) {
            add('com', m[1]);
        } else if (m[2]) {
            add('str', m[2]);
        } else if (m[3]) {
            add('pre', m[3]);
            if (m[4]) {
                const path = m[4].trimStart();
                add(null, m[4].slice(0, -path.length));
                add('str', path);
            }
        } else if (m[5]) {
            add(rules.tags ? 'tag' : null, m[5]);
        } else if (m[6]) {
            add('num', m[6]);
        } else if (m[7]) {
            const w = m[7];
            let kind = null;
            if (rules.keyword(w)) {
                kind = 'kw';
            } else if (rules.type(w)) {
                kind = 'type';
            } else if (w.startsWith('gl_')) {
                kind = 'var';
            } else if (/^[A-Z][A-Z\d_]+$/.test(w)) {
                kind = 'const';
            } else if (m[8]) {
                kind = 'fn';
            }
            add(kind, w);
        } else {
            add(null, m[0]);
        }
    }
    add(null, text.slice(pos));
    if (plain) {
        frag.append(plain);
    }
    return frag;
}

// the language of a source file
function langOf(file) {
    if (file.endsWith('.glsl')) {
        return 'glsl';
    }
    return /\.(cc|cpp|hh|hpp)$/.test(file) ? 'cpp' : 'c';
}
