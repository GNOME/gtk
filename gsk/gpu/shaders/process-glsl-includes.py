#!/usr/bin/env python3

import argparse
import sys
import re
import os

loaded_files = []

check_defines = [ 'VULKAN', 'GSK_PREAMBLE' ]

def load (path, includes):
    if (path in loaded_files):
        return

    loaded_files.append (path)
    skipping = ''

    with open(path) as f:
        lines = f.readlines()
        for line in lines:
            if skipping:
                match = re.search (r"^#endif /\* (.*) \*/$", line)
                if match and match.group(1) == skipping:
                    skipping = ''
                continue

            match = re.search (r"^#define (.*)$", line)
            if match and match.group(1) in check_defines:
                check_defines.remove (match.group(1))
                print (line, end="")
                continue

            match = re.search (r"^#ifdef (.*)$", line)
            if match and match.group(1) in check_defines:
                skipping = match.group(1)
                continue

            match = re.search (r"^#include \"(.*)\"$", line)
            if match:
                for dir in [os.path.dirname(path)] + includes:
                    file = os.path.join (dir, match.group (1))
                    if not os.path.isfile (file):
                        continue
                    load (file, includes)
                    break
            else:
                print (line, end="")

parser = argparse.ArgumentParser()
parser.add_argument ('-I', '--include', type=str, action='append', help='Add include directory')
parser.add_argument ('FILES', nargs='*', help='Input files')
args = parser.parse_args()

for path in args.FILES:
    load (path, args.include if args.include else [])

