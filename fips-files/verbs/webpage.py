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

# webpage text colors
Keys = {
    'webgl': {
        'title': 'Sokol WebGL',
        'cross_name': 'webgpu',
        'cross_url': 'https://floooh.github.io/sokol-webgpu',
        'body_bg_color': '#292d3e',
        'thumb_bg_color': '#4a4d62',
        'text_color': '#1ABC9C',
    },
    'webgpu': {
        'title': 'Sokol WebGPU',
        'cross_name': 'webgl',
        'cross_url': 'https://floooh.github.io/sokol-html5',
        'body_bg_color': '#292d3e',
        'thumb_bg_color': '#4a4d62',
        'text_color': '#3498DB',
    }
}

# build configuration
def get_build_config(api):
    if api == 'webgl':
        return 'sapp-webgl2-wasm-ninja-release'
    else:
        return 'sapp-wgpu-wasm-ninja-release'

# sample attributes
samples = [
    [ 'clear', 'clear-sapp.c', None],
    [ 'triangle', 'triangle-sapp.c', 'triangle-sapp.glsl'],
    [ 'triangle-bufferless', 'triangle-bufferless-sapp.c', 'triangle-bufferless-sapp.glsl'],
    [ 'quad', 'quad-sapp.c', 'quad-sapp.glsl'],
    [ 'bufferoffsets', 'bufferoffsets-sapp.c', 'bufferoffsets-sapp.glsl'],
    [ 'cube', 'cube-sapp.c', 'cube-sapp.glsl'],
    [ 'noninterleaved', 'noninterleaved-sapp.c', 'noninterleaved-sapp.glsl'],
    [ 'texcube', 'texcube-sapp.c', 'texcube-sapp.glsl' ],
    [ 'vertexpull', 'vertexpull-sapp.c', 'vertexpull-sapp.glsl' ],
    [ 'sbuftex', 'sbuftex-sapp.c', 'sbuftex-sapp.glsl' ],
    [ 'shapes', 'shapes-sapp.c', 'shapes-sapp.glsl'],
    [ 'shapes-transform', 'shapes-transform-sapp.c', 'shapes-transform-sapp.glsl'],
    [ 'offscreen', 'offscreen-sapp.c', 'offscreen-sapp.glsl' ],
    [ 'offscreen-msaa', 'offscreen-msaa-sapp.c', 'offscreen-msaa-sapp.glsl' ],
    [ 'instancing', 'instancing-sapp.c', 'instancing-sapp.glsl' ],
    [ 'instancing-pull', 'instancing-pull.c', 'instancing-pull.glsl' ],
    [ 'mrt', 'mrt-sapp.c', 'mrt-sapp.glsl' ],
    [ 'mrt-pixelformats', 'mrt-pixelformats-sapp.c', 'mrt-pixelformats-sapp.glsl' ],
    [ 'arraytex', 'arraytex-sapp.c', 'arraytex-sapp.glsl' ],
    [ 'tex3d', 'tex3d-sapp.c', 'tex3d-sapp.glsl' ],
    [ 'dyntex3d', 'dyntex3d-sapp.c', 'dyntex3d-sapp.glsl' ],
    [ 'dyntex', 'dyntex-sapp.c', 'dyntex-sapp.glsl'],
    [ 'basisu', 'basisu-sapp.c', None ],
    [ 'cubemap-jpeg', 'cubemap-jpeg-sapp.c', 'cubemap-jpeg-sapp.glsl' ],
    [ 'cubemaprt', 'cubemaprt-sapp.c', 'cubemaprt-sapp.glsl' ],
    [ 'miprender', 'miprender-sapp.c', 'miprender-sapp.glsl' ],
    [ 'layerrender', 'layerrender-sapp.c', 'layerrender-sapp.glsl' ],
    [ 'primtypes', 'primtypes-sapp.c', 'primtypes-sapp.glsl'],
    [ 'uvwrap', 'uvwrap-sapp.c', 'uvwrap-sapp.glsl'],
    [ 'mipmap', 'mipmap-sapp.c', 'mipmap-sapp.glsl'],
    [ 'uniformtypes', 'uniformtypes-sapp.c', 'uniformtypes-sapp.glsl' ],
    [ 'blend', 'blend-sapp.c', 'blend-sapp.glsl' ],
    [ 'sdf', 'sdf-sapp.c', 'sdf-sapp.glsl'],
    [ 'shadows', 'shadows-sapp.c', 'shadows-sapp.glsl'],
    [ 'shadows-depthtex', 'shadows-depthtex-sapp.c', 'shadows-depthtex-sapp.glsl'],
    [ 'imgui', 'imgui-sapp.cc', None ],
    [ 'imgui-dock', 'imgui-dock-sapp.cc', None ],
    [ 'imgui-highdpi', 'imgui-highdpi-sapp.cc', None ],
    [ 'cimgui', 'cimgui-sapp.c', None ],
    [ 'imgui-images', 'imgui-images-sapp.c', None ],
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
    [ 'drawcallperf', 'drawcallperf-sapp.c', 'drawcallperf-sapp.glsl'],
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
    [ 'ozz-storagebuffer', 'ozz-storagebuffer-sapp.c', 'ozz-storagebuffer-sapp.glsl' ],
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
    "nb2_posx.jpg",
    "nb2_negx.jpg",
    "nb2_posy.jpg",
    "nb2_negy.jpg",
    "nb2_posz.jpg",
    "nb2_negz.jpg",
]

#-------------------------------------------------------------------------------
def cwebp(src_path, dst_path):
    cmd_line = 'cwebp -quiet -q 80 {} -o {}'.format(src_path, dst_path)
    print('> {}'.format(cmd_line))
    subprocess.call(cmd_line, shell=True)

#-------------------------------------------------------------------------------
def deploy_webpage(fips_dir, proj_dir, api, webpage_dir) :
    wasm_deploy_dir = util.get_deploy_dir(fips_dir, 'sokol-samples', get_build_config(api))
    src_root_path = proj_dir + '/webpage/'
    dst_root_path = webpage_dir + '/'

    # build the thumbnail gallery
    content = ''
    for sample in samples:
        name = sample[0]
        log.info('> adding thumbnail for {}'.format(name))
        url = "{}-sapp.html".format(name)
        ui_url = "{}-sapp-ui.html".format(name)
        img_name = name + '.webp'
        if not os.path.exists(src_root_path + name + '.jpg'):
            img_name = 'dummy.webp'
        content += '<div class="thumb">'
        content += '  <div class="thumb-title">{}</div>'.format(name)
        if os.path.exists(wasm_deploy_dir + '/' + name + '-sapp-ui.js'):
            content += '<a class="img-btn-link" href="{}"><div class="img-btn">UI</div></a>'.format(ui_url)
        content += '  <div class="img-frame"><a href="{}"><img class="image" src="{}"></img></a></div>'.format(url,img_name)
        content += '</div>\n'

    # populate the html template, and write to the build directory
    with open(src_root_path + 'index.html', 'r') as f:
        templ = Template(f.read())
    keys = Keys[api]
    html = templ.safe_substitute(
        samples = content,
        title = keys['title'],
        cross_name = keys['cross_name'],
        cross_url = keys['cross_url'],
        body_bg_color = keys['body_bg_color'],
        thumb_bg_color = keys['thumb_bg_color'],
        text_color = keys['text_color'])

    with open(dst_root_path + 'index.html', 'w') as f :
        f.write(html)

    # copy other required files
    for name in ['favicon.png']:
        log.info('> copy file: {}'.format(name))
        shutil.copy(src_root_path + name, dst_root_path + name)

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
                    shutil.copy(src_path, dst_root_path)
                with open(src_root_path + 'wasm.html', 'r') as f :
                    templ = Template(f.read())
                src_url = GitHubSamplesURL + source
                if glsl is None:
                    glsl_url = "."
                    glsl_hidden = "hidden"
                else:
                    glsl_url = GitHubSamplesURL + glsl
                    glsl_hidden = ""
                html = templ.safe_substitute(name=name, prog=name+'-'+postfix, source=src_url, glsl=glsl_url, hidden=glsl_hidden)
                with open('{}{}-{}.html'.format(dst_root_path, name, postfix), 'w') as f :
                    f.write(html)

    # copy assets from deploy directory
    for asset in assets:
        log.info('> copy asset file: {}'.format(asset))
        src_path = '{}/{}'.format(wasm_deploy_dir, asset)
        if os.path.isfile(src_path):
            shutil.copy(src_path, dst_root_path)
        else:
            log.warn('!!! file {} not found!'.format(src_path))

    # copy the screenshots
    cwebp(src_root_path + 'dummy.jpg', dst_root_path + 'dummy.webp')
    for sample in samples :
        src_img_path = src_root_path + sample[0] + '.jpg'
        dst_img_path = dst_root_path + sample[0] + '.webp'
        if os.path.exists(src_img_path):
            cwebp(src_img_path, dst_img_path)

#-------------------------------------------------------------------------------
def build_deploy_webpage(fips_dir, proj_dir, api, rebuild) :
    # if webpage dir exists, clear it first
    build_config = get_build_config(api)
    proj_build_dir = util.get_deploy_root_dir(fips_dir, 'sokol-samples')
    webpage_dir = '{}/sokol-webpage-{}'.format(proj_build_dir, api)
    if rebuild :
        if os.path.isdir(webpage_dir) :
            shutil.rmtree(webpage_dir)
    if not os.path.isdir(webpage_dir) :
        os.makedirs(webpage_dir)

    # compile samples
    project.gen(fips_dir, proj_dir, build_config)
    project.build(fips_dir, proj_dir, build_config)

    # deploy the webpage
    deploy_webpage(fips_dir, proj_dir, api, webpage_dir)

    log.colored(log.GREEN, 'Generated Samples web page under {}.'.format(webpage_dir))

#-------------------------------------------------------------------------------
def serve_webpage(fips_dir, proj_dir, api) :
    proj_build_dir = util.get_deploy_root_dir(fips_dir, 'sokol-samples')
    webpage_dir = '{}/sokol-webpage-{}'.format(proj_build_dir, api)
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
        action = args[0]
        api = 'webgl'
        if len(args) > 1:
            api = args[1]
        if api not in ['webgl', 'webgpu']:
            log.error("Invalid param '{}', expected 'webgl' or 'webgpu'".format(api))
        if action == 'build' :
            build_deploy_webpage(fips_dir, proj_dir, api, False)
        elif action == 'rebuild' :
            build_deploy_webpage(fips_dir, proj_dir, api,  True)
        elif action == 'serve' :
            serve_webpage(fips_dir, proj_dir, api)
        else :
            log.error("Invalid param '{}', expected 'build' or 'serve'".format(action))
    else :
        log.error("Params 'build|rebuild|serve [webgl|webgpu]' expected")

#-------------------------------------------------------------------------------
def help() :
    log.info(log.YELLOW +
             'fips webpage build [webgl|webgpu]\n' +
             'fips webpage rebuild [webgl|webgpu]\n' +
             'fips webpage serve [webgl|webgpu]\n' +
             log.DEF +
             '    build sokol samples webpage')
