#-------------------------------------------------------------------------------
#   dump.py
#   Dump binary files into C arrays.
#-------------------------------------------------------------------------------

Version = 1

import sys
import os.path
import yaml
import genutil

#-------------------------------------------------------------------------------
def get_file_path(filename, file_path) :
    '''
    Returns absolute path to an input file, given file name and 
    another full file path in the same directory.
    '''
    return '{}/{}'.format(os.path.dirname(file_path), filename)

#-------------------------------------------------------------------------------
def get_file_cname(filename) :
    return 'dump_{}'.format(os.path.splitext(filename)[0])

#-------------------------------------------------------------------------------
def gen_header(out_hdr, files) :
    with open(out_hdr, 'w') as f:
        f.write('#pragma once\n')
        f.write('// #version:{}#\n'.format(Version))
        f.write('// machine generated, do not edit!\n')
        items = {}
        for file in files :
            file_path = get_file_path(file, out_hdr)
            if os.path.isfile(file_path) :
                with open(file_path, 'rb') as src_file:
                    file_data = src_file.read()
                    file_name = get_file_cname(file)
                    file_size = os.path.getsize(file_path)
                    items[file_name] = file_size
                    f.write('unsigned char {}[{}] = {{\n'.format(file_name, file_size))               
                    num = 0
                    for byte in file_data :
                        if sys.version_info[0] >= 3:
                            f.write(hex(ord(chr(byte))) + ', ')
                        else:
                            f.write(hex(ord(byte)) + ', ')
                        num += 1
                        if 0 == num%16:
                            f.write('\n')
                    f.write('\n};\n')
            else :
                genutil.fmtError("Input file not found: '{}'".format(file_path))
        f.write('typedef struct { const char* name; const uint8_t* ptr; int size; } dump_item;\n')
        f.write('#define DUMP_NUM_ITEMS ({})\n'.format(len(items)))
        f.write('dump_item dump_items[DUMP_NUM_ITEMS] = {\n')
        for name,size in items.items():
            f.write('{{ "{}", {}, {} }},\n'.format(name[5:], name, size))
        f.write('};\n')

#-------------------------------------------------------------------------------
def generate(input, out_src, out_hdr) :
    if genutil.isDirty(Version, [input], [out_hdr]) :
        with open(input, 'r') as f :
            desc = yaml.load(f)
        gen_header(out_hdr, desc['files'])
