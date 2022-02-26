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
    gdk_parser.add_argument('--broadway',
                              action='store_const',
                              const=1,
                              help='GDK with Broadway backend')
    gdk_parser.add_argument('--host',
                            required=True,
                            help='Build type')
    base_pc.setup(argv, gdk_parser)

    atk_min_ver = '2.15.1'
    cairo_min_ver = '1.14.0'
    gdk_pixbuf_min_ver = '2.30.0'
    gdk_win32_sys_libs = '-lgdi32 -limm32 -lshell32 -lole32 -lwinmm -ldwmapi -lwinspool -lcomctl32 -lcomdlg32'
    cairo_libs = '-lcairo-gobject -lcairo '
    glib_min_ver = '2.45.8'

    gdk_backends = 'win32'
    gio_package = 'gio-2.0 >= ' + glib_min_ver
    broadway_extra_libs = ''

    gdk_args = gdk_parser.parse_args()
    if getattr(gdk_args, 'broadway', None) is 1:
        # On Visual Studio, we link to zlib1.lib
        broadway_extra_libs = ' -lzlib1'
        gdk_backends += ' broadway'

    pkg_replace_items = {'@GTK_API_VERSION@': '3.0',
                         '@GDK_BACKENDS@': gdk_backends}

    pkg_required_packages = 'gdk-pixbuf-2.0 >= ' + gdk_pixbuf_min_ver

    gdk_pc_replace_items = {'@GDK_PACKAGES@': gio_package + ' ' + \
                                              'pangowin32 pangocairo' + ' ' + \
                                              pkg_required_packages,
                            '@GDK_PRIVATE_PACKAGES@': gio_package,
                            '@GDK_EXTRA_LIBS@': cairo_libs + gdk_win32_sys_libs + broadway_extra_libs,
                            '@GDK_EXTRA_CFLAGS@': '',
                            'gdk-3': 'gdk-3.0'}

    gtk_pc_replace_items = {'@host@': gdk_args.host,
                            '@GTK_BINARY_VERSION@': '3.0.0',
                            '@GTK_PACKAGES@': 'atk >= ' + atk_min_ver + ' ' + \
                                              pkg_required_packages + ' ' + \
                                              gio_package,
                            '@GTK_PRIVATE_PACKAGES@': 'atk',
                            '@GTK_EXTRA_CFLAGS@': '',
                            '@GTK_EXTRA_LIBS@': '',
                            '@GTK_EXTRA_CFLAGS@': '',
                            'gtk-3': 'gtk-3.0'}

    gail_pc_replace_items = {'gailutil-3': 'gailutil-3.0'}

    pkg_replace_items.update(base_pc.base_replace_items)
    gdk_pc_replace_items.update(pkg_replace_items)
    gtk_pc_replace_items.update(pkg_replace_items)
    gail_pc_replace_items.update(base_pc.base_replace_items)

    # Generate gdk-3.0.pc
    replace_multi(base_pc.top_srcdir + '/gdk-3.0.pc.in',
                  base_pc.srcdir + '/gdk-3.0.pc',
                  gdk_pc_replace_items)

    # Generate gtk+-3.0.pc
    replace_multi(base_pc.top_srcdir + '/gtk+-3.0.pc.in',
                  base_pc.srcdir + '/gtk+-3.0.pc',
                  gtk_pc_replace_items)

    # Generate gail-3.0.pc
    replace_multi(base_pc.top_srcdir + '/gail-3.0.pc.in',
                  base_pc.srcdir + '/gail-3.0.pc',
                  gail_pc_replace_items)

if __name__ == '__main__':
    sys.exit(main(sys.argv))
