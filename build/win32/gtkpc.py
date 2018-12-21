#!/usr/bin/python
#
# Utility script to generate .pc files for GTK+
# for Visual Studio builds, to be used for
# building introspection files

# Author: Fan, Chun-wei
# Date: April 26, 2016

import os
import sys
import argparse

from replace import replace_multi, replace
from pc_base import BasePCItems

def main(argv):
    base_pc = BasePCItems()

    gdk_parser = argparse.ArgumentParser(description='Setup basic .pc file info')
    gdk_parser.add_argument('--host',
                            required=True,
                            help='Build type')
    base_pc.setup(argv, gdk_parser)

    atk_min_ver = '1.29.2'
    gdk_pixbuf_min_ver = '2.21.0'
    gdk_win32_sys_libs = '-lgdi32 -limm32 -lshell32 -lole32 -lwinmm'
    gdk_additional_libs = '-lcairo'
    glib_min_ver = '2.28.0'

    cairo_backends = 'cairo-win32'
    gdktarget = 'win32'
    gio_package = 'gio-2.0 >= ' + glib_min_ver

    gdk_args = gdk_parser.parse_args()

    pkg_replace_items = {'@GTK_API_VERSION@': '2.0',
                         '@gdktarget@': gdktarget}

    pkg_required_packages = 'gdk-pixbuf-2.0 >= ' + gdk_pixbuf_min_ver + ' '

    gdk_pc_replace_items = {'@GDK_PACKAGES@': gio_package + ' ' + \
                                              'pangowin32 pangocairo' + ' ' + \
                                              pkg_required_packages,
                            '@GDK_PRIVATE_PACKAGES@': gio_package + ' ' + cairo_backends,
                            '@GDK_EXTRA_LIBS@': gdk_additional_libs + ' ' + gdk_win32_sys_libs,
                            '@GDK_EXTRA_CFLAGS@': ''}

    gtk_pc_replace_items = {'@host@': gdk_args.host,
                            '@GTK_BINARY_VERSION@': '2.10.0',
                            '@GTK_PACKAGES@': 'atk >= ' + atk_min_ver + ' ' + \
                                              pkg_required_packages + ' ' + \
                                              gio_package,
                            '@GTK_PRIVATE_PACKAGES@': 'atk',
                            '@GTK_EXTRA_CFLAGS@': '',
                            '@GTK_EXTRA_LIBS@': ' ' + gdk_additional_libs,
                            '@GTK_EXTRA_CFLAGS@': ''}

    pkg_replace_items.update(base_pc.base_replace_items)
    gdk_pc_replace_items.update(pkg_replace_items)
    gtk_pc_replace_items.update(pkg_replace_items)

    # Generate gdk-2.0.pc
    replace_multi(base_pc.top_srcdir + '/gdk-2.0.pc.in',
                  base_pc.srcdir + '/gdk-2.0.pc',
                  gdk_pc_replace_items)

    # Generate gtk+-2.0.pc
    replace_multi(base_pc.top_srcdir + '/gtk+-2.0.pc.in',
                  base_pc.srcdir + '/gtk+-2.0.pc',
                  gtk_pc_replace_items)

    # Generate gail.pc
    replace_multi(base_pc.top_srcdir + '/gail.pc.in',
                  base_pc.srcdir + '/gail.pc',
                  pkg_replace_items)

if __name__ == '__main__':
    sys.exit(main(sys.argv))
