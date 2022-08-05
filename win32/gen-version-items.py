#!/usr/bin/python3

# Generate various items with version info

# Author: Fan, Chun-wei
# Date: July 25, 2019

import argparse
import os
import re
import sys

from replace import replace_multi, replace

def get_srcroot():
    if not os.path.isabs(__file__):
        path = os.path.abspath(__file__)
    else:
        path = __file__
    dirname = os.path.dirname(path)
    return os.path.abspath(os.path.join(dirname, '..'))

def get_version(srcroot):
    ver = {}
    RE_VERSION = re.compile(r'^m4_define\(\[(gtk_\w+)\],\s*\[(\d+)\]\)')
    with open(os.path.join(srcroot, 'configure.ac'), 'r') as ac:
        for i in ac:
            mo = RE_VERSION.search(i)
            if mo:
                ver[mo.group(1).upper()] = int(mo.group(2))
    ver['GTK_VERSION'] = '%d.%d.%d' % (ver['GTK_MAJOR_VERSION'],
                                       ver['GTK_MINOR_VERSION'],
                                       ver['GTK_MICRO_VERSION'])
    return ver

def main(argv):
    srcdir = os.path.dirname(__file__)
    top_srcdir = os.path.join(srcdir, os.pardir)
    parser = argparse.ArgumentParser(description='Generate various items with version info')
    parser.add_argument('--version', help='Version of the package')
    parser.add_argument('--interface-age', help='Interface age of the package')
    parser.add_argument('--source', help='Source file template to process',
                        required=True)
    parser.add_argument('--output', '-o', help='Output generated file location',
                        required=True)
    args = parser.parse_args()
    version_info = get_version(get_srcroot())

    # If version and/or interface-age were specified, use them,
    # otherwise use the info we have from configure.ac.
    if args.version is not None:
        gtk_version = args.version
    else:
        gtk_version = version_info['GTK_VERSION']
    if args.interface_age is not None:
        interface_age = args.interface_age
    else:
        interface_age = version_info['GTK_INTERFACE_AGE']

    version_parts = gtk_version.split('.')
    # (100 * gtk_minor_version + gtk_micro_version - gtk_interface_age)
    binary_age = (int(version_parts[1]) * 100) + int(version_parts[2])
    lt_current = (int(version_parts[1]) * 100) + int(version_parts[2]) - int(interface_age)
    lt_age = binary_age - int(interface_age)

    version_info_replace_items = {'@GTK_MAJOR_VERSION@':    version_parts[0],
                                  '@GTK_MINOR_VERSION@':    version_parts[1],
                                  '@GTK_MICRO_VERSION@':    version_parts[2],
                                  '@GTK_API_VERSION@':      '3.0',
                                  '@GTK_VERSION@':          gtk_version,
                                  '@GTK_BINARY_AGE@':       str(binary_age),
                                  '@GTK_INTERFACE_AGE@':    str(interface_age),
                                  '@GETTEXT_PACKAGE@':      'gtk30',
                                  '@LT_CURRENT_MINUS_AGE@': str(lt_current - lt_age)}

    replace_multi(args.source, args.output, version_info_replace_items)

if __name__ == '__main__':
    sys.exit(main(sys.argv))
