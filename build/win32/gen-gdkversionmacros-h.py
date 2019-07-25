#!/usr/bin/python3

# Generate gdk/gdkversionmacros.h

# Author: Fan, Chun-wei
# Date: July 25, 2019

import os
import sys
import argparse

from replace import replace_multi, replace

def main(argv):
    srcdir = os.path.dirname(__file__)
    top_srcdir = os.path.join(srcdir, os.pardir, os.pardir)
    parser = argparse.ArgumentParser(description='Generate gdkversionmacros.h')
    parser.add_argument('--version', help='Version of the package',
                        required=True)
    args = parser.parse_args()
    gdk_sourcedir = os.path.join(top_srcdir, 'gdk')
    version_parts = args.version.split('.')

    gdkversionmacro_replace_items = {'@GTK_MAJOR_VERSION@': version_parts[0],
                                     '@GTK_MINOR_VERSION@': version_parts[1],
                                     '@GTK_MICRO_VERSION@': version_parts[2]}

    replace_multi(os.path.join(gdk_sourcedir, 'gdkversionmacros.h.in'),
                  os.path.join(gdk_sourcedir, 'gdkversionmacros.h'),
                  gdkversionmacro_replace_items)

if __name__ == '__main__':
    sys.exit(main(sys.argv))
