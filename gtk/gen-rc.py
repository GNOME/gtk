#! /usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
import os

out_file = sys.argv[1]
in_file = sys.argv[2]
old_msvc = sys.argv[3]

with open(out_file, 'w', encoding='utf-8') as o:
    if old_msvc is not None and old_msvc == "1":
        o.write("#define ISOLATION_AWARE_ENABLED 1\n")

    with open(in_file, 'r', encoding='utf-8') as f:
        for line in f:
            o.write(line)

    o.write('\n')
    o.write('ISOLATIONAWARE_MANIFEST_RESOURCE_ID RT_MANIFEST libgtk3.manifest')
