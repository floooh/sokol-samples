"""fips verb to build the samples webpage"""

import os
import yaml
import shutil
import subprocess
import glob
from string import Template

from mod import log, util, project

# webpage template arguments
GitHubSamplesURL = 'https://github.com/floooh/sokol-samples/tree/master/sapp/'

# build configuration
BuildConfig = 'sapp-webgl2-wasm-ninja-release'

# sample attributes
samples = [
    [ 'clear', 'clear-sapp.c', None],
    [ 'triangle', 'triangle-sapp.c', 'triangle-sapp.glsl'],
    [ 'quad', 'quad-sapp.c', 'quad-sapp.glsl'],
    [ 'bufferoffsets', 'bufferoffsets-sapp.c', 'bufferoffsets-sapp.glsl'],
    [ 'cube', 'cube-sapp.c', 'cube-sapp.glsl'],
    [ 'noninterleaved', 'noninterleaved-sapp.c', 'noninterleaved-sapp.glsl'],
    [ 'texcube', 'texcube-sapp.c', 'texcube-sapp.glsl' ],
    [ 'shapes', 'shapes-sapp.c', 'shapes-sapp.glsl'],
    [ 'shapes-transform', 'shapes-transform-sapp.c', 'shapes-transform-sapp.glsl'],
    [ 'offscreen', 'offscreen-sapp.c', 'offscreen-sapp.glsl' ],
    [ 'instancing', 'instancing-sapp.c', 'instancing-sapp.glsl' ],
    [ 'mrt', 'mrt-sapp.c', 'mrt-sapp.glsl' ],
    [ 'mrt-pixelformats', 'mrt-pixelformats-sapp.c', 'mrt-pixelformats-sapp.glsl' ],
    [ 'arraytex', 'arraytex-sapp.c', 'arraytex-sapp.glsl' ],
    [ 'tex3d', 'tex3d-sapp.c', 'tex3d-sapp.glsl' ],
    [ 'dyntex', 'dyntex-sapp.c', 'dyntex-sapp.glsl'],
    [ 'basisu', 'basisu-sapp.c', None ],
    [ 'primtypes', 'primtypes-sapp.c', 'primtypes-sapp.glsl'],
    [ 'uvwrap', 'uvwrap-sapp.c', 'uvwrap-sapp.glsl'],
    [ 'mipmap', 'mipmap-sapp.c', 'mipmap-sapp.glsl'],
    [ 'uniformtypes', 'uniformtypes-sapp.c', 'uniformtypes-sapp.glsl' ],
    [ 'blend', 'blend-sapp.c', 'blend-sapp.glsl' ],
    [ 'sdf', 'sdf-sapp.c', 'sdf-sapp.glsl'],
    [ 'shadows', 'shadows-sapp.c', 'shadows-sapp.glsl'],
    [ 'imgui', 'imgui-sapp.cc', None ],
    [ 'imgui-dock', 'imgui-dock-sapp.cc', None ],
    [ 'imgui-highdpi', 'imgui-highdpi-sapp.cc', None ],
    [ 'cimgui', 'cimgui-sapp.c', None ],
    [ 'imgui-usercallback', 'imgui-usercallback-sapp.c', 'imgui-usercallback-sapp.glsl'],
    [ 'nuklear', 'nuklear-sapp.c', None ],
    [ 'nuklear-images', 'nuklear-images-sapp.c', None ],
    [ 'sgl-microui', 'sgl-microui-sapp.c', None],
    [ 'fontstash', 'fontstash-sapp.c', None],
    [ 'fontstash-layers', 'fontstash-layers-sapp.c', 'fontstash-layers-sapp.glsl'],
    [ 'debugtext', 'debugtext-sapp.c', None],
    [ 'debugtext-printf', 'debugtext-printf-sapp.c', None],
    [ 'debugtext-userfont', 'debugtext-userfont-sapp.c', None],
    [ 'debugtext-context', 'debugtext-context-sapp.c', 'debugtext-context-sapp.glsl'],
    [ 'debugtext-layers', 'debugtext-layers-sapp.c', None],
    [ 'events', 'events-sapp.cc', None],
    [ 'icon', 'icon-sapp.c', None ],
    [ 'droptest', 'droptest-sapp.c', None],
    [ 'pixelformats', 'pixelformats-sapp.c', None],
    [ 'saudio', 'saudio-sapp.c', None],
    [ 'modplay', 'modplay-sapp.c', None],
    [ 'noentry', 'noentry-sapp.c', 'noentry-sapp.glsl' ],
    [ 'restart', 'restart-sapp.c', 'restart-sapp.glsl' ],
    [ 'sgl', 'sgl-sapp.c', None ],
    [ 'sgl-lines', 'sgl-lines-sapp.c', None ],
    [ 'sgl-points', 'sgl-points-sapp.c', None ],
    [ 'sgl-context', 'sgl-context-sapp.c', None ],
    [ 'loadpng', 'loadpng-sapp.c', 'loadpng-sapp.glsl'],
    [ 'plmpeg', 'plmpeg-sapp.c', 'plmpeg-sapp.glsl'],
    [ 'cgltf', 'cgltf-sapp.c', 'cgltf-sapp.glsl'],
    [ 'ozz-anim', 'ozz-anim-sapp.cc', None ],
    [ 'ozz-skin', 'ozz-skin-sapp.cc', 'ozz-skin-sapp.glsl'],
    [ 'shdfeatures', 'shdfeatures-sapp.c', 'shdfeatures-sapp.glsl'],
    [ 'spine-simple', 'spine-simple-sapp.c', None ],
    [ 'spine-inspector', 'spine-inspector-sapp.c', None ],
    [ 'spine-layers', 'spine-layers-sapp.c', None ],
    [ 'spine-skinsets', 'spine-skinsets-sapp.c', None ],
    [ 'spine-switch-skinsets', 'spine-switch-skinsets-sapp.c', None ],
    [ 'spine-contexts', 'spine-contexts-sapp.c', None ],
]

# assets that must also be copied
assets = [
    "comsi.s3m",
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
    "DroidSansJapanese.ttf",
    "ozz_anim_skeleton.ozz",
    "ozz_anim_animation.ozz",
    "ozz_skin_skeleton.ozz",
    "ozz_skin_animation.ozz",
    "ozz_skin_mesh.ozz",
    "raptor-pma.atlas",
    "raptor-pma.png",
    "raptor-pro.skel",
    "spineboy-pro.json",
    "spineboy.atlas",
    "spineboy.png",
    "alien-pma.atlas",
    "alien-pma.png",
    "alien-pro.skel",
    "speedy-pma.atlas",
    "speedy-pma.png",
    "speedy-ess.skel",
    "mix-and-match-pma.atlas",
    "mix-and-match-pma.png",
    "mix-and-match-pro.skel",
]

#-------------------------------------------------------------------------------
def deploy_webpage(fips_dir, proj_dir, webpage_dir) :
    wasm_deploy_dir = util.get_deploy_dir(fips_dir, 'sokol-samples', BuildConfig)

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
        content += '<div class="thumb">'
        content += '  <div class="thumb-title">{}</div>'.format(name)
        if os.path.exists(wasm_deploy_dir + '/' + name + '-sapp-ui.js'):
            content += '<a class="img-btn-link" href="{}"><div class="img-btn">UI</div></a>'.format(ui_url)
        content += '  <div class="img-frame"><a href="{}"><img class="image" src="{}"></img></a></div>'.format(url,img_name)
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
        else:
            log.warn('!!! file {} not found!'.format(src_path))

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
    proj_build_dir = util.get_deploy_root_dir(fips_dir, 'sokol-samples')
    webpage_dir = '{}/sokol-webpage'.format(proj_build_dir)
    if rebuild :
        if os.path.isdir(webpage_dir) :
            shutil.rmtree(webpage_dir)
    if not os.path.isdir(webpage_dir) :
        os.makedirs(webpage_dir)

    # compile samples
    project.gen(fips_dir, proj_dir, BuildConfig)
    project.build(fips_dir, proj_dir, BuildConfig)

    # deploy the webpage
    deploy_webpage(fips_dir, proj_dir, webpage_dir)

    log.colored(log.GREEN, 'Generated Samples web page under {}.'.format(webpage_dir))

#-------------------------------------------------------------------------------
def serve_webpage(fips_dir, proj_dir) :
    proj_build_dir = util.get_deploy_root_dir(fips_dir, 'sokol-samples')
    webpage_dir = '{}/sokol-webpage'.format(proj_build_dir)
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
             'fips webpage build\n' +
             'fips webpage rebuild\n' +
             'fips webpage serve\n' +
             log.DEF +
             '    build sokol samples webpage')
