#!/usr/bin/env python3

import argparse
import sys

parser = argparse.ArgumentParser()
parser.add_argument('--array-name', help='The name of the array variable')
parser.add_argument('--output', metavar='FILE', help='Output file',
                    type=argparse.FileType('w'),
                    default=sys.stdout)
parser.add_argument('input', metavar='FILE', help='The input file',
                    type=argparse.FileType('r'), nargs='+')

args = parser.parse_args()

args.output.write('static const char {}[] = {{\n'.format(args.array_name))
for input in args.input:
    for line in input:
        for ch in line:
            args.output.write('  0x{:02x},\n'.format(ord(ch)))

args.output.write('};')
