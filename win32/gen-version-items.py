#!/usr/bin/python3

# Generate various items with version info

# Author: Fan, Chun-wei
# Date: July 25, 2019

import os
import sys
import argparse

from replace import replace_multi, replace

def main(argv):
    srcdir = os.path.dirname(__file__)
    top_srcdir = os.path.join(srcdir, os.pardir)
    parser = argparse.ArgumentParser(description='Generate various items with version info')
    parser.add_argument('--version', help='Version of the package',
                        required=True)
    parser.add_argument('--interface-age', help='Interface age of the package',
                        required=True)
    parser.add_argument('--source', help='Source file template to process',
                        required=True)
    parser.add_argument('--output', '-o', help='Output generated file location',
                        required=True)
    args = parser.parse_args()
    gdk_sourcedir = os.path.join(top_srcdir, 'gdk')
    version_parts = args.version.split('.')
    # (100 * gtk_minor_version + gtk_micro_version - gtk_interface_age)
    binary_age = (int(version_parts[1]) * 100) + int(version_parts[2])
    lt_current = (int(version_parts[1]) * 100) + int(version_parts[2]) - int(args.interface_age)
    lt_age = binary_age - int(args.interface_age)

    version_info_replace_items = {'@GTK_MAJOR_VERSION@':    version_parts[0],
                                  '@GTK_MINOR_VERSION@':    version_parts[1],
                                  '@GTK_MICRO_VERSION@':    version_parts[2],
                                  '@GTK_API_VERSION@':      '3.0',
                                  '@GTK_VERSION@':          args.version,
                                  '@GTK_BINARY_AGE@':       str(binary_age),
                                  '@GTK_INTERFACE_AGE@':    args.interface_age,
                                  '@LT_CURRENT_MINUS_AGE@': str(lt_current - lt_age)}

    replace_multi(args.source, args.output, version_info_replace_items)

if __name__ == '__main__':
    sys.exit(main(sys.argv))
