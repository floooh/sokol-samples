"""fips verb to build the samples webpage"""

import os
import yaml 
import shutil
import subprocess
import glob
from string import Template

from mod import log, util, project, emscripten, android

# sample attributes
samples = [
    [ 'clear', 'clear-emsc.c' ],
    [ 'triangle', 'triangle-emsc.c' ],
    [ 'quad', 'quad-emsc.c' ],
    [ 'cube', 'cube-emsc.c' ],
    [ 'texcube', 'texcube-emsc.c' ],
    [ 'offscreen', 'offscreen-emsc.c' ],
    [ 'instancing', 'instancing-emsc.c' ],
    [ 'mrt', 'mrt-emsc.c' ],
    [ 'arraytex', 'arraytex-emsc.c' ],
    [ 'dyntex', 'dyntex-emsc.c'],
    [ 'mipmap', 'mipmap-emsc.c'],
    [ 'imgui', 'imgui-emsc.cc' ]
]

# webpage template arguments
GitHubSamplesURL = 'https://github.com/floooh/sokol-samples/tree/master/html5/'
DocTitle = 'Sokol Samples'
Title = 'Sokol'
Subtitle = 'samples'

Separator = 'simple-separator'
GameSeparator = 'simple-separator'
BackgroundColor = '#00E676'

# build configuration
EmscConfig = 'webgl2-emsc-ninja-release'
WasmConfig = 'webgl2-wasm-ninja-release'

#-------------------------------------------------------------------------------
def deploy_webpage(fips_dir, proj_dir, webpage_dir) :
    """builds the final webpage under under fips-deploy/sokol-webpage"""
    ws_dir = util.get_workspace_dir(fips_dir)

    # create directories
    for platform in ['asmjs', 'wasm'] :
        platform_dir = '{}/{}'.format(webpage_dir, platform)
        if not os.path.isdir(platform_dir) :
            os.makedirs(platform_dir)

    # build the thumbnail gallery
    content = ''
    for sample in samples :
        name = sample[0]
        log.info('> adding thumbnail for {}'.format(name))
        img_name = name + '.jpg'
        img_path = proj_dir + '/webpage/' + img_name
        if not os.path.exists(img_path):
            img_name = 'dummy.jpg'
            img_path = proj_dir + 'webpage/dummy.jpg'
        content += '<div class="thumb">\n'
        content += '  <div class="thumb-title">{}</div>\n'.format(name)
        content += '  <div class="img-frame"><a href="asmjs/{}-emsc.html"><img class="image" src="{}"></img></a></div>\n'.format(name,img_name)
        content += '  <div class="thumb-bar">\n'
        content += '    <ul class="thumb-list">\n'
        content += '      <li class="thumb-item"><a class="thumb-link" href="asmjs/{}-emsc.html">asm.js</a></li>\n'.format(name)
        content += '      <li class="thumb-item"><a class="thumb-link" href="wasm/{}-emsc.html">wasm</a></li>\n'.format(name)
        content += '    </ul>\n'
        content += '  </div>\n'
        content += '</div>\n'

    # populate the html template, and write to the build directory
    with open(proj_dir + '/webpage/index.html', 'r') as f :
        templ = Template(f.read())
    html = templ.safe_substitute(doctitle=DocTitle, title=Title, subtitle=Subtitle, samples=content, separator=Separator)
    with open(webpage_dir + '/index.html', 'w') as f :
        f.write(html)

    # and the same with the CSS template
    with open(proj_dir + '/webpage/style.css', 'r') as f :
        templ = Template(f.read())
    css = templ.safe_substitute(background=BackgroundColor)
    with open(webpage_dir +'/style.css', 'w') as f :
        f.write(css)

    # copy other required files
    for name in ['dummy.jpg', 'emsc.js', 'wasm.js', 'favicon.png'] :
        log.info('> copy file: {}'.format(name))
        shutil.copy(proj_dir + '/webpage/' + name, webpage_dir + '/' + name)

    # generate emscripten HTML pages
    if emscripten.check_exists(fips_dir) :
        emsc_deploy_dir = '{}/fips-deploy/sokol-samples/{}'.format(ws_dir, EmscConfig)
        for sample in samples :
            name = sample[0]
            source = sample[1]
            log.info('> generate emscripten HTML page: {}'.format(name))
            for ext in ['js', 'html.mem'] :
                src_path = '{}/{}-emsc.{}'.format(emsc_deploy_dir, name, ext)
                if os.path.isfile(src_path) :
                    shutil.copy(src_path, '{}/asmjs/'.format(webpage_dir))
            with open(proj_dir + '/webpage/emsc.html', 'r') as f :
                templ = Template(f.read())
            src_url = GitHubSamplesURL + source
            html = templ.safe_substitute(name=name+'-emsc', source=src_url, separator=GameSeparator)
            with open('{}/asmjs/{}-emsc.html'.format(webpage_dir, name, name), 'w') as f :
                f.write(html)

    # generate WebAssembly HTML pages
    if emscripten.check_exists(fips_dir) :
        wasm_deploy_dir = '{}/fips-deploy/sokol-samples/{}'.format(ws_dir, WasmConfig)
        for sample in samples :
            name = sample[0]
            source = sample[1]
            log.info('> generate wasm HTML page: {}'.format(name))
            for ext in ['js', 'wasm.mappedGlobals'] :
                src_path = '{}/{}-emsc.{}'.format(wasm_deploy_dir, name, ext)
                if os.path.isfile(src_path) :
                    shutil.copy(src_path, '{}/wasm/'.format(webpage_dir))
            for ext in ['wasm'] :
                src_path = '{}/{}-emsc.{}'.format(wasm_deploy_dir, name, ext)
                if os.path.isfile(src_path) :
                    shutil.copy(src_path, '{}/wasm/{}-emsc.{}.txt'.format(webpage_dir, name, ext))
            with open(proj_dir + '/webpage/wasm.html', 'r') as f :
                templ = Template(f.read())
            src_url = GitHubSamplesURL + source
            html = templ.safe_substitute(name=name+'-emsc', source=src_url, separator=GameSeparator)
            with open('{}/wasm/{}-emsc.html'.format(webpage_dir, name), 'w') as f :
                f.write(html)

    # copy the screenshots
    for sample in samples :
        img_name = sample[0] + '.jpg'
        img_path = proj_dir + '/webpage/' + img_name
        if os.path.exists(img_path):
            log.info('> copy screenshot: {}'.format(img_name))
            shutil.copy(img_path, webpage_dir + '/' + img_name)

#-------------------------------------------------------------------------------
def build_deploy_webpage(fips_dir, proj_dir, rebuild) :
    # if webpage dir exists, clear it first
    ws_dir = util.get_workspace_dir(fips_dir)
    webpage_dir = '{}/fips-deploy/sokol-webpage'.format(ws_dir)
    if rebuild :
        if os.path.isdir(webpage_dir) :
            shutil.rmtree(webpage_dir)
    if not os.path.isdir(webpage_dir) :
        os.makedirs(webpage_dir)

    # compile samples
    if emscripten.check_exists(fips_dir) :
        project.gen(fips_dir, proj_dir, EmscConfig)
        project.build(fips_dir, proj_dir, EmscConfig)
    if emscripten.check_exists(fips_dir) :
        project.gen(fips_dir, proj_dir, WasmConfig)
        project.build(fips_dir, proj_dir, WasmConfig)
    
    # deploy the webpage
    deploy_webpage(fips_dir, proj_dir, webpage_dir)

    log.colored(log.GREEN, 'Generated Samples web page under {}.'.format(webpage_dir))

#-------------------------------------------------------------------------------
def serve_webpage(fips_dir, proj_dir) :
    ws_dir = util.get_workspace_dir(fips_dir)
    webpage_dir = '{}/fips-deploy/sokol-webpage'.format(ws_dir)
    p = util.get_host_platform()
    if p == 'osx' :
        try :
            subprocess.call(
                'open http://localhost:8000 ; python {}/mod/httpserver.py'.format(fips_dir),
                cwd = webpage_dir, shell=True)
        except KeyboardInterrupt :
            pass
    elif p == 'win':
        try:
            subprocess.call(
                'cmd /c start http://localhost:8000 && python {}/mod/httpserver.py'.format(fips_dir),
                cwd = webpage_dir, shell=True)
        except KeyboardInterrupt:
            pass
    elif p == 'linux':
        try:
            subprocess.call(
                'xdg-open http://localhost:8000; python {}/mod/httpserver.py'.format(fips_dir),
                cwd = webpage_dir, shell=True)
        except KeyboardInterrupt:
            pass

#-------------------------------------------------------------------------------
def run(fips_dir, proj_dir, args) :
    if len(args) > 0 :
        if args[0] == 'build' :
            build_deploy_webpage(fips_dir, proj_dir, False)
        elif args[0] == 'rebuild' :
            build_deploy_webpage(fips_dir, proj_dir, True)
        elif args[0] == 'serve' :
            serve_webpage(fips_dir, proj_dir)
        else :
            log.error("Invalid param '{}', expected 'build' or 'serve'".format(args[0]))
    else :
        log.error("Param 'build' or 'serve' expected")

#-------------------------------------------------------------------------------
def help() :
    log.info(log.YELLOW +
             'fips webpage build\n' +
             'fips webpage rebuild\n' +
             'fips webpage serve\n' +
             log.DEF +
             '    build sokol samples webpage')

