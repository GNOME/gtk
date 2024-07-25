#!/usr/bin/env python3

import sys
import re
import os

loaded_files = []

check_defines = [ 'VULKAN' ]

def load (path):
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
                load (os.path.join (os.path.dirname(path), match.group(1)))
            else:
                print (line, end="")

load (sys.argv[1])

