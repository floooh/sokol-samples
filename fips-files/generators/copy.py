'''
A generic file-copy generator script for asset files.
'''
Version = 3

import genutil as util
import subprocess
import shutil
import yaml
import os
import platform

#-------------------------------------------------------------------------------
def copy_files(src_dir, dst_dir, yml):
    for filename in yml['files']:
        src = src_dir + filename
        dst = dst_dir + filename
        print("## cp '{}' => '{}'".format(filename, dst))
        if not os.path.exists(os.path.dirname(dst)):
            os.makedirs(os.path.dirname(dst))
        try:
            shutil.copyfile(src, dst)
        except IOError as err:
            # show a proper error if file copying fails
            util.fmtError("Failed to copy file '{}' with '{}'".format(err.filename, err.strerror))


#-------------------------------------------------------------------------------
def gen_header(out_hdr, yml):
    with open(out_hdr, 'w') as f:
        f.write('#pragma once\n')
        f.write('//------------------------------------------------------------------------------\n')
        f.write('// #version:{}#\n'.format(Version))
        f.write('// machine generated, do not edit!\n')
        f.write('//------------------------------------------------------------------------------\n')
        f.write('// JUST A DUMMY FILE, NOTHING TO SEE HERE\n')

#-------------------------------------------------------------------------------
def check_dirty(src_root_path, input, out_hdr, yml):
    out_files = [out_hdr]
    in_files  = [input]
    for filename in yml['files']:
        in_files.append(os.path.abspath(src_root_path + filename))
    return util.isDirty(Version, in_files, out_files)

#-------------------------------------------------------------------------------
def generate(input, out_src, out_hdr, args):
    with open(input, 'r') as f:
        try:
            yml = yaml.load(f)
        except yaml.YAMLError as exc:
            # show a proper error if YAML parsing fails
            util.setErrorLocation(exc.problem_mark.name, exc.problem_mark.line-1)
            util.fmtError('YAML error: {}'.format(exc.problem))
    util.setErrorLocation(input, 1)
    src_root_path = os.path.dirname(input) + '/'
    if 'root' in yml:
        src_root_path += yml['root'] + '/'
    dst_dir = args['deploy_dir'] + '/'
    if 'options' in yml:
        if util.getEnv('target_platform') == 'ios' and 'ios' in yml['options']:
            if 'dst_dir' in yml['options']['ios']:
                dst_dir += yml['options']['ios']['dst_dir'] + '/'
        if util.getEnv('target_platform') == 'osx' and 'macos' in yml['options']:
            if 'dst_dir' in yml['options']['macos']:
                dst_dir += yml['options']['macos']['dst_dir'] + '/'
        del yml['options']
    if not os.path.exists(dst_dir):
        os.makedirs(dst_dir)
    if check_dirty(src_root_path, input, out_hdr, yml):
        copy_files(src_root_path, dst_dir, yml)
        gen_header(out_hdr, yml)
