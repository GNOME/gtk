#!/usr/bin/python
# vim: encoding=utf-8
#expand *.in files
import os
import sys
import re
import optparse

def parent_dir(path):
    if not os.path.isabs(path):
        path = os.path.abspath(path)
    if os.path.isfile(path):
        path = os.path.dirname(path)
    return os.path.split(path)[0]

def check_output_type (btype):
    print_bad_type = False
    output_type = -1
    if (btype is None):
        output_type = -1
        print_bad_type = False
    elif (btype == "vs9"):
        output_type = 1
    elif (btype == "vs10"):
        output_type = 2
    elif (btype == "nmake-exe"):
        output_type = 3
    else:
        output_type = -1
        print_bad_type = True
    if (output_type == -1):
        if (print_bad_type is True):
            print ("The entered output build file type '%s' is not valid" % btype)
        else:
            print ("Output build file type is not specified.\nUse -t <type> to specify the output build file type.")
        print ("Valid output build file types are: nmake-exe, vs9 , vs10")
    return output_type

def read_vars_from_AM(path, vars = {}, conds = {}, filters = None):
    '''
    path: path to the Makefile.am
    vars: predefined variables
    conds: condition variables for Makefile
    filters: if None, all variables defined are returned,
             otherwise, it is a list contains that variables should be returned
    '''
    cur_vars = vars.copy()
    RE_AM_VAR_REF = re.compile(r'\$\((\w+?)\)')
    RE_AM_VAR = re.compile(r'^\s*(\w+)\s*=(.*)$')
    RE_AM_INCLUDE = re.compile(r'^\s*include\s+(\w+)')
    RE_AM_VAR_ADD = re.compile(r'^\s*(\w+)\s*\+=(.*)$')
    RE_AM_CONTINUING = re.compile(r'\\\s*$')
    RE_AM_IF = re.compile(r'^\s*if\s+(\w+)')
    RE_AM_IFNOT = re.compile(r'^\s*if\s!+(\w+)')
    RE_AM_ELSE = re.compile(r'^\s*else')
    RE_AM_ENDIF = re.compile(r'^\s*endif')
    def am_eval(cont):
        return RE_AM_VAR_REF.sub(lambda x: cur_vars.get(x.group(1), ''), cont)
    with open(path, 'r') as f:
        contents = f.readlines()
    #combine continuing lines
    i = 0
    ncont = []
    while i < len(contents):
        line = contents[i]
        if RE_AM_CONTINUING.search(line):
            line = RE_AM_CONTINUING.sub('', line)
            j = i + 1
            while j < len(contents) and RE_AM_CONTINUING.search(contents[j]):
                line += RE_AM_CONTINUING.sub('', contents[j])
                j += 1
            else:
                if j < len(contents):
                    line += contents[j]
            i = j
        else:
            i += 1
        ncont.append(line)

    #include, var define, var evaluation
    i = -1
    skip = False
    oldskip = []
    while i < len(ncont) - 1:
        i += 1
        line = ncont[i]
        mo = RE_AM_IF.search(line)
        if mo:
            oldskip.append(skip)
            skip = False if mo.group(1) in conds and conds[mo.group(1)] \
                         else True
            continue
        mo = RE_AM_IFNOT.search(line)
        if mo:
            oldskip.append(skip)
            skip = False if mo.group(1) not in conds and conds[mo.group(1)] \
                         else True
            continue
        mo = RE_AM_ELSE.search(line)
        if mo:
            skip = not skip
            continue
        mo = RE_AM_ENDIF.search(line)
        if mo:
            if oldskip:
                skip = oldskip.pop()
            continue
        if not skip:
            mo = RE_AM_INCLUDE.search(line)
            if mo:
                cur_vars.update(read_vars_from_AM(am_eval(mo.group(1)), cur_vars, conds, None))
                continue
            mo = RE_AM_VAR.search(line)
            if mo:
                cur_vars[mo.group(1)] = am_eval(mo.group(2).strip())
                continue
            mo = RE_AM_VAR_ADD.search(line)
            if mo:
                try:
                    cur_vars[mo.group(1)] += ' '
                except KeyError:
                    cur_vars[mo.group(1)] = ''
                cur_vars[mo.group(1)] += am_eval(mo.group(2).strip())
                continue

    #filter:
    if filters != None:
        ret = {}
        for i in filters:
            ret[i] = cur_vars.get(i, '')
        return ret
    else:
        return cur_vars

def process_include(src, dest, includes):
    RE_INCLUDE = re.compile(r'^\s*#include\s+"(.*)"')
    with open(src, 'r') as s:
        with open(dest, 'w') as d:
            for i in s:
                mo = RE_INCLUDE.search(i)
                if mo:
                    target = ''
                    for j in includes:
                        #print "searching in ", j
                        if mo.group(1) in os.listdir(j):
                            target = os.path.join(j, mo.group(1))
                            break
                    if not target:
                        raise Exception("Couldn't find include file %s" % mo.group(1))
                    else:
                        with open(target, 'r') as t:
                            for inc in t.readlines():
                                d.write(inc)
                else:
                    d.write(i)

#Generate the source files listing that is used
def generate_src_list (srcroot, srcdir, filters_src, filter_conds, filter_c, mk_am_file):
    mkfile = ''
    if mk_am_file is None or mk_am_file == '':
        mkfile = 'Makefile.am'
    else:
        mkfile = mk_am_file
    vars = read_vars_from_AM(os.path.join(srcdir, mkfile),
                             vars = {'top_srcdir': srcroot},
                             conds = filter_conds,
                             filters = filters_src)
    files = []
    for src_filters_item in filters_src:
        files += vars[src_filters_item].split()
    if filter_c is True:
        sources = [i for i in files if i.endswith('.c') ]
        return sources
    else:
        return files

# Generate the Visual Studio 2008 Project Files from the templates
def gen_vs9_project (projname, srcroot, srcdir_name, sources_list):
    vs_file_list_dir = os.path.join (srcroot, 'build', 'win32')

    with open (os.path.join (vs_file_list_dir,
              projname + '.sourcefiles'), 'w') as vs9srclist:
        for i in sources_list:
            vs9srclist.write ('\t\t\t<File RelativePath="..\\..\\..\\' + srcdir_name + '\\' + i.replace('/', '\\') + '" />\n')

    process_include (os.path.join(srcroot, 'build', 'win32', 'vs9', projname + '.vcprojin'),
                     os.path.join(srcroot, 'build', 'win32', 'vs9', projname + '.vcproj'),
                     includes = [vs_file_list_dir])

    os.unlink(os.path.join(srcroot, 'build', 'win32', projname + '.sourcefiles'))

# Generate the Visual Studio 2010 Project Files from the templates
def gen_vs10_project (projname, srcroot, srcdir_name, sources_list):
    vs_file_list_dir = os.path.join (srcroot, 'build', 'win32')

    with open (os.path.join (vs_file_list_dir,
              projname + '.vs10.sourcefiles'), 'w') as vs10srclist:
        for j in sources_list:
            vs10srclist.write ('    <ClCompile Include="..\\..\\..\\' + srcdir_name + '\\' + j.replace('/', '\\') + '" />\n')

    with open (os.path.join (vs_file_list_dir,
              projname + '.vs10.sourcefiles.filters'), 'w') as vs10srclist_filter:
        for k in sources_list:
             vs10srclist_filter.write ('    <ClCompile Include="..\\..\\..\\' + srcdir_name + '\\' + k.replace('/', '\\') + '"><Filter>Source Files</Filter></ClCompile>\n')

    process_include (os.path.join(srcroot, 'build', 'win32', 'vs10', projname + '.vcxprojin'),
                     os.path.join(srcroot, 'build', 'win32', 'vs10', projname + '.vcxproj'),
                     includes = [vs_file_list_dir])
    process_include (os.path.join(srcroot, 'build', 'win32', 'vs10', projname + '.vcxproj.filtersin'),
                     os.path.join(srcroot, 'build', 'win32', 'vs10', projname + '.vcxproj.filters'),
                     includes = [vs_file_list_dir])

    os.unlink(os.path.join(srcroot, 'build', 'win32', projname + '.vs10.sourcefiles'))
    os.unlink(os.path.join(srcroot, 'build', 'win32', projname + '.vs10.sourcefiles.filters'))

def gen_vs_inst_list (projname, srcroot, srcdirs, inst_lists, destdir_names, isVS9):
    vs_file_list_dir = os.path.join (srcroot, 'build', 'win32')
    vsver = ''
    vsprops_line_ending = ''
    vsprops_file_ext = ''
    if isVS9 is True:
        vsver = '9'
        vsprops_line_ending = '&#x0D;&#x0A;\n'
        vsprops_file_ext = '.vsprops'
    else:
        vsver = '10'
        vsprops_line_ending = '\n\n'
        vsprops_file_ext = '.props'

    with open (os.path.join (vs_file_list_dir,
              projname + '.vs' + vsver + 'instfiles'), 'w') as vsinstlist:

        for file_list, srcdir, dir_name in zip (inst_lists, srcdirs, destdir_names):
            for i in file_list:
                vsinstlist.write ('copy ..\\..\\..\\' +
                                  srcdir + '\\' +
                                  i.replace ('/', '\\') +
                                  ' $(CopyDir)\\' +
                                  dir_name +
                                  vsprops_line_ending)
    process_include (os.path.join(srcroot, 'build', 'win32', 'vs' + vsver, projname + '-install' + vsprops_file_ext + 'in'),
                     os.path.join(srcroot, 'build', 'win32', 'vs' + vsver, projname + '-install' + vsprops_file_ext),
                     includes = [vs_file_list_dir])

    os.unlink(os.path.join (vs_file_list_dir, projname + '.vs' + vsver + 'instfiles'))

def generate_nmake_makefiles(srcroot, srcdir, base_name, makefile_name, progs_list):
    file_list_dir = os.path.join (srcroot, 'build', 'win32')

    with open (os.path.join (file_list_dir,
              base_name + '_progs'), 'w') as proglist:
        for i in progs_list:
            proglist.write ('\t' + i + '$(EXEEXT)\t\\\n')


    process_include (os.path.join(srcdir, makefile_name + 'in'),
                    os.path.join(srcdir, makefile_name),
                    includes = [file_list_dir])

    os.unlink(os.path.join (file_list_dir, base_name + '_progs'))

