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
    [ 'clear', 'clear-sapp.c', None],
    [ 'triangle', 'triangle-sapp.c', 'triangle-sapp.glsl'],
    [ 'quad', 'quad-sapp.c', 'quad-sapp.glsl'],
    [ 'bufferoffsets', 'bufferoffsets-sapp.c', 'bufferoffsets-sapp.glsl'],
    [ 'cube', 'cube-sapp.c', 'cube-sapp.glsl'],
    [ 'noninterleaved', 'noninterleaved-sapp.c', 'noninterleaved-sapp.glsl'],
    [ 'texcube', 'texcube-sapp.c', 'texcube-sapp.glsl' ],
    [ 'offscreen', 'offscreen-sapp.c', 'offscreen-sapp.glsl' ],
    [ 'instancing', 'instancing-sapp.c', 'instancing-sapp.glsl' ],
    [ 'mrt', 'mrt-sapp.c', 'mrt-sapp.glsl' ],
    [ 'arraytex', 'arraytex-sapp.c', 'arraytex-sapp.glsl' ],
    [ 'dyntex', 'dyntex-sapp.c', 'dyntex-sapp.glsl'],
    [ 'uvwrap', 'uvwrap-sapp.c', 'uvwrap-sapp.glsl'],
    [ 'mipmap', 'mipmap-sapp.c', 'mipmap-sapp.glsl'],
    [ 'blend', 'blend-sapp.c', 'blend-sapp.glsl' ],
    [ 'sdf', 'sdf-sapp.c', 'sdf-sapp.glsl'],
    [ 'imgui', 'imgui-sapp.cc', None ],
    [ 'imgui-highdpi', 'imgui-highdpi-sapp.cc', None ],
    [ 'cimgui', 'cimgui-sapp.c', None ],
    [ 'imgui-usercallback', 'imgui-usercallback-sapp.c', 'imgui-usercallback-sapp.glsl'],
    [ 'sgl-microui', 'sgl-microui-sapp.c', None],
    [ 'fontstash', 'fontstash-sapp.c', None],
    [ 'events', 'events-sapp.cc', None],
    [ 'pixelformats', 'pixelformats-sapp.c', None],
    [ 'pixelformats-gles2', 'pixelformats-sapp.c', None],
    [ 'saudio', 'saudio-sapp.c', None],
    [ 'modplay', 'modplay-sapp.c', None],
    [ 'noentry', 'noentry-sapp.c', 'noentry-sapp.glsl' ],
    [ 'sgl', 'sgl-sapp.c', None ],
    [ 'sgl-lines', 'sgl-lines-sapp.c', None ],
    [ 'loadpng', 'loadpng-sapp.c', 'loadpng-sapp.glsl'],
    [ 'plmpeg', 'plmpeg-sapp.c', 'plmpeg-sapp.glsl'],
    [ 'cgltf', 'cgltf-sapp.c', 'cgltf-sapp.glsl'],
    [ 'cubemaprt', 'cubemaprt-sapp.c', 'cubemaprt-sapp.glsl']
]

# assets that must also be copied
assets = [
    "baboon.png",
    "bjork-all-is-full-of-love.mpg",
    "DamagedHelmet.gltf",
    "DamagedHelmet.bin",
    "Default_AO.basis",
    "Default_albedo.basis",
    "Default_emissive.basis",
    "Default_metalRoughness.basis",
    "Default_normal.basis",
    "DroidSerif-Regular.ttf",
    "DroidSerif-Italic.ttf",
    "DroidSerif-Bold.ttf",
    "DroidSansJapanese.ttf"
]

# webpage template arguments
GitHubSamplesURL = 'https://github.com/floooh/sokol-samples/tree/master/sapp/'

# build configuration
BuildConfig = 'sapp-wgpu-wasm-vscode-release'

#-------------------------------------------------------------------------------
def deploy_webpage(fips_dir, proj_dir, webpage_dir) :
    """builds the final webpage under under fips-deploy/sokol-webpage"""
    ws_dir = util.get_workspace_dir(fips_dir)
    wasm_deploy_dir = '{}/fips-deploy/sokol-samples/{}'.format(ws_dir, BuildConfig)

    # create directories
    if not os.path.exists(webpage_dir):
        os.makedirs(webpage_dir)

    # build the thumbnail gallery
    content = ''
    for sample in samples:
        name = sample[0]
        log.info('> adding thumbnail for {}'.format(name))
        url = "{}-sapp.html".format(name)
        ui_url = "{}-sapp-ui.html".format(name)
        img_name = name + '.jpg'
        img_path = proj_dir + '/webpage/' + img_name
        if not os.path.exists(img_path):
            img_name = 'dummy.jpg'
            img_path = proj_dir + 'webpage/dummy.jpg'
        content += '<div class="thumb">\n'
        content += '  <div class="thumb-title">{}</div>\n'.format(name)
        if os.path.exists(wasm_deploy_dir + '/' + name + '-sapp-ui.js'):
            content += '<a class="img-btn-link" href="{}"><div class="img-btn">UI</div></a>'.format(ui_url)
        content += '  <div class="img-frame"><a href="{}"><img class="image" src="{}"></img></a></div>\n'.format(url,img_name)
        content += '</div>\n'

    # populate the html template, and write to the build directory
    with open(proj_dir + '/webpage/index.html', 'r') as f:
        templ = Template(f.read())
    html = templ.safe_substitute(samples=content)
    with open(webpage_dir + '/index.html', 'w') as f :
        f.write(html)

    # copy other required files
    for name in ['dummy.jpg', 'favicon.png']:
        log.info('> copy file: {}'.format(name))
        shutil.copy(proj_dir + '/webpage/' + name, webpage_dir + '/' + name)

    # generate WebAssembly HTML pages
    if emscripten.check_exists(fips_dir):
        for sample in samples :
            name = sample[0]
            source = sample[1]
            glsl = sample[2]
            log.info('> generate wasm HTML page: {}'.format(name))
            for postfix in ['sapp', 'sapp-ui']:
                for ext in ['wasm', 'js'] :
                    src_path = '{}/{}-{}.{}'.format(wasm_deploy_dir, name, postfix, ext)
                    if os.path.isfile(src_path) :
                        shutil.copy(src_path, '{}/'.format(webpage_dir))
                    with open(proj_dir + '/webpage/wasm.html', 'r') as f :
                        templ = Template(f.read())
                    src_url = GitHubSamplesURL + source
                    if glsl is None:
                        glsl_url = "."
                        glsl_hidden = "hidden"
                    else:
                        glsl_url = GitHubSamplesURL + glsl
                        glsl_hidden = ""
                    html = templ.safe_substitute(name=name, prog=name+'-'+postfix, source=src_url, glsl=glsl_url, hidden=glsl_hidden)
                    with open('{}/{}-{}.html'.format(webpage_dir, name, postfix), 'w') as f :
                        f.write(html)

    # copy assets from deploy directory
    for asset in assets:
        log.info('> copy asset file: {}'.format(asset))
        src_path = '{}/{}'.format(wasm_deploy_dir, asset)
        if os.path.isfile(src_path):
            shutil.copy(src_path, webpage_dir)

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
        project.gen(fips_dir, proj_dir, BuildConfig)
        project.build(fips_dir, proj_dir, BuildConfig)

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
                'http-server -c-1 -g -o'.format(fips_dir),
                cwd = webpage_dir, shell=True)
        except KeyboardInterrupt :
            pass
    elif p == 'win':
        try:
            subprocess.call(
                'http-server -c-1 -g -o'.format(fips_dir),
                cwd = webpage_dir, shell=True)
        except KeyboardInterrupt:
            pass
    elif p == 'linux':
        try:
            subprocess.call(
                'http-server -c-1 -g -o'.format(fips_dir),
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
             'fips wgpu_webpage build\n' +
             'fips wgpu_webpage rebuild\n' +
             'fips wgpu_webpage serve\n' +
             log.DEF +
             '    build sokol samples webpage')

