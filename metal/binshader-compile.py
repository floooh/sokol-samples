#
# run 'python binshader-compile.py' on OSX to compile
# the example metal shader files 'binshader-vs.metal' and
# 'binshader-fs.metal' into the header 'binshader-metal.h'
# with hexdumps of the shader binary code.
#

import subprocess, tempfile, binascii

SDK = None

def xcrun(cmdline):
    cmd = ['xcrun', '--sdk', SDK, '--run']
    cmd.extend(cmdline)
    print(' '.join(cmd))
    subprocess.call(cmd)

def cc(metal_file, air_file):
    if SDK == 'macosx':
        min_version = '-mmacosx-version-min=10.11'
        std = '-std=osx-metal1.1'
    else:
        min_version = '-miphoneos-version-min=9.0'
        std = '-std=ios-metal1.0'
    xcrun(['metal', '-arch', 'air64', '-emit-llvm', '-ffast-math', '-c',
        '-o', air_file,
        min_version, std, metal_file])

def ar(air_file, lib_file):
    xcrun(['metal-ar', 'r', lib_file, air_file])

def link(lib_file, out_file):
    xcrun(['metallib', '-o', out_file, lib_file])

def write_hexdump(outf, name, in_file):
    with open(in_file, 'rb') as inf:
        data = inf.read()
    hexdata = binascii.hexlify(data)
    outf.write('const uint8_t {}[] = {{\n'.format(name))
    for i in range(0, len(data)):
        outf.write('0x{}{},'.format(hexdata[i*2], hexdata[i*2+1]))
        if (i % 16) == 15:
            outf.write('\n')
    outf.write('\n};\n')

###-- start
tmpdir = tempfile.gettempdir()

vs_air_tmp = tmpdir + "/vs.air"
vs_lib_tmp = tmpdir + "/vs.lib"
vs_out_tmp = tmpdir + "/vs.out"
fs_air_tmp = tmpdir + "/fs.air"
fs_lib_tmp = tmpdir + "/fs.lib"
fs_out_tmp = tmpdir + "/fs.out"

for SDK in ['macosx', 'iphoneos']:
    cc('binshader-vs.metal', vs_air_tmp)
    ar(vs_air_tmp, vs_lib_tmp)
    link(vs_lib_tmp, vs_out_tmp)
    cc('binshader-fs.metal', fs_air_tmp)
    ar(fs_air_tmp, fs_lib_tmp)
    link(fs_lib_tmp, fs_out_tmp)
    with open('binshader-metal-{}.h'.format(SDK), 'w') as f:
        f.write('#pragma once\n')
        write_hexdump(f, 'vs_bytecode', vs_out_tmp)
        write_hexdump(f, 'fs_bytecode', fs_out_tmp)
