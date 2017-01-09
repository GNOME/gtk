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
    gdk_parser.add_argument('--vulkan',
                              action='store_const',
                              const=1,
                              help='GSK with Vulkan renderer')
    gdk_parser.add_argument('--host',
                            required=True,
                            help='Build type')
    base_pc.setup(argv, gdk_parser)

    atk_min_ver = '2.15.1'
    cairo_min_ver = '1.15.2'
    gdk_pixbuf_min_ver = '2.30.0'
    gdk_win32_sys_libs = '-lgdi32 -limm32 -lshell32 -lole32 -Wl,-luuid -lwinmm -ldwmapi'
    glib_min_ver = '2.49.4'
    epoxy_min_ver = '1.0'
    graphene_min_ver = '1.2'

    cairo_backends = 'cairo-win32'
    gdk_backends = 'win32'
    gio_package = 'gio-2.0 >= ' + glib_min_ver
    vulkan_extra_libs = ''

    gdk_args = gdk_parser.parse_args()
    if getattr(gdk_args, 'vulkan', None) is 1:
        # On Visual Studio, we link to zlib1.lib
        vulkan_extra_libs = ' -lvulkan-1'
        gdk_backends += ' vulkan'
        cairo_backends += ' cairo'

    pkg_replace_items = {'@GTK_API_VERSION@': '4.0',
                         '@GDK_BACKENDS@': gdk_backends}

    pkg_required_packages = 'gdk-pixbuf >= ' + gdk_pixbuf_min_ver + ' ' + \
                            'cairo >= ' + cairo_min_ver + ' ' + \
                            'cairo-gobject >= ' + cairo_min_ver

    gtk_pc_replace_items = {'@host@': gdk_args.host,
                            '@GTK_BINARY_VERSION@': '4.0.0',
                            '@GDK_PACKAGES@': gio_package + ' ' + \
                                              'pangowin32 pangocairo' + ' ' + \
                                              pkg_required_packages,
                            '@GSK_PACKAGES@': pkg_required_packages + ' ' + \
                                              'graphene-1.0 >= ' + graphene_min_ver,
                            '@GTK_PACKAGES@': 'atk >= ' + atk_min_ver + ' ' + \
                                              pkg_required_packages + ' ' + \
                                              gio_package,
                            '@GDK_PRIVATE_PACKAGES@': gio_package + ' ' + cairo_backends,
                            '@GSK_PRIVATE_PACKAGES@': 'epoxy >= ' + epoxy_min_ver,
                            '@GTK_PRIVATE_PACKAGES@': 'atk',
                            '@GDK_EXTRA_CFLAGS@': '',
                            '@GSK_EXTRA_CFLAGS@': '',
                            '@GTK_EXTRA_CFLAGS@': '',
                            '@GDK_EXTRA_LIBS@': gdk_win32_sys_libs + vulkan_extra_libs,
                            '@GSK_EXTRA_LIBS@': '',
                            '@GTK_EXTRA_LIBS@': ''}

    pkg_replace_items.update(base_pc.base_replace_items)
    gtk_pc_replace_items.update(pkg_replace_items)

    # Generate gtk+-4.0.pc
    replace_multi(base_pc.top_srcdir + '/gtk+-4.0.pc.in',
                  base_pc.srcdir + '/gtk+-4.0.pc',
                  gtk_pc_replace_items)

if __name__ == '__main__':
    sys.exit(main(sys.argv))
