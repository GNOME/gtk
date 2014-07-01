#!/usr/bin/python
# vim: encoding=utf-8
# Generate the file lists for processing with g-ir-scanner
import os
import sys
import re
import string
import subprocess
import optparse

from msvcfiles import read_vars_from_AM

def gen_gdk_filelist(srcroot, subdir, dest):
    vars = read_vars_from_AM(os.path.join(srcroot, subdir, 'Makefile.am'),
                             vars = {},
                             conds = {},
                             filters = ['gdk_h_sources', 'gdk_c_sources'])

    vars['gdk_enums'] = 'gdkenumtypes.c gdkenumtypes.h'

    files = vars['gdk_h_sources'].split() + \
            vars['gdk_c_sources'].split() + \
            vars['gdk_enums'].split()

    sources = [i for i in files if (i != 'gdkkeysyms-compat.h')]

    with open(dest, 'w') as d:
        for i in sources:
            d.write(srcroot + '\\' + subdir + '\\' + i.replace('/', '\\') + '\n')

def gen_gdkwin32_filelist(srcroot, subdir, dest):
    vars = read_vars_from_AM(os.path.join(srcroot, subdir, 'Makefile.am'),
                             vars = {},
                             conds = {'HAVE_INTROSPECTION': True,
                                      'OS_WIN32': True},
                             filters = ['w32_introspection_files'])

    files = vars['w32_introspection_files'].split()

    with open(dest, 'w') as d:
        for i in files:
            d.write(srcroot + '\\' + subdir + '\\' + i.replace('/', '\\') + '\n')

def gen_gtk_filelist(srcroot, subdir, dest):
    vars = read_vars_from_AM(os.path.join(srcroot, 'gtk', 'Makefile.am'),
                             vars = {},
                             conds = {'USE_WIN32': True,
                                      'USE_QUARTZ': False,
                                      'USE_X11': False,
                                      'USE_EXTERNAL_ICON_CACHE': False},
                             filters = ['gtkinclude_HEADERS',
                                        'a11yinclude_HEADERS',
                                        'deprecatedinclude_HEADERS',
                                        'gtk_base_c_sources',
                                        'gtk_clipboard_dnd_c_sources'])

    vars['gtk_other_src'] = 'gtkprintoperation-win32.c gtktypebuiltins.h gtktypebuiltins.c'

    files = vars['gtkinclude_HEADERS'].split() + \
            vars['a11yinclude_HEADERS'].split() + \
            vars['deprecatedinclude_HEADERS'].split() + \
            vars['gtk_base_c_sources'].split() + \
			vars['gtk_clipboard_dnd_c_sources'].split() + \
            vars['gtk_other_src'].split()

    sources = [i for i in files if not (i.endswith('private.h')) and i != 'gtktextdisplay.h' and i != 'gtktextlayout.h']

    with open(dest, 'w') as d:
        for i in sources:
            d.write(srcroot + '\\' + subdir + '\\' + i.replace('/', '\\') + '\n')

def main(argv):
    srcroot = '..'
    subdir_gdk = 'gdk'
    subdir_gtk = 'gtk'

    gen_gdk_filelist(srcroot, subdir_gdk, 'gdk_list')
    gen_gdkwin32_filelist(srcroot, subdir_gdk, 'gdkwin32_list')
    gen_gtk_filelist(srcroot, subdir_gtk, 'gtk_list')
    return 0

if __name__ == '__main__':
    sys.exit(main(sys.argv))
