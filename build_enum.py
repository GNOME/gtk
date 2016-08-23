#!/usr/bin/env python3

# This is in its own file rather than inside meson.build
# because a) mixing the two is ugly and b) trying to
# make special characters such as \n go through all
# backends is a fool's errand.

import sys, os, shutil, subprocess
# [perl, glib-mkenums]
cmd = [sys.argv[1], sys.argv[2]]
template = sys.argv[3]
ofilename = sys.argv[4]
headers = sys.argv[5:]


arg_array = ['--template', template];

pc = subprocess.Popen(cmd + arg_array + headers, stdout=subprocess.PIPE)
(stdo, _) = pc.communicate()
if pc.returncode != 0:
    sys.exit(pc.returncode)
open(ofilename, 'wb').write(stdo)
