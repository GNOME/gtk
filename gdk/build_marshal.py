#!/usr/bin/env python3

# This is in its own file rather than inside meson.build
# because a) mixing the two is ugly and b) trying to
# make special characters such as \n go through all
# backends is a fool's errand.

import sys, os, shutil, subprocess

# HORRIBLE, use current_source_dir() as an argument instead.
h_array = ['--prefix=_gdk_marshal', '--header']

c_array = ['--prefix=_gdk_marshal', '--body']

# [genmarshal, infile, outfile]
cmd = [sys.argv[1]]
ifilename = sys.argv[2]
ofilename = sys.argv[3]

if ofilename.endswith('.h'):
    arg_array = h_array
else:
    arg_array = c_array

pc = subprocess.Popen(cmd + [ifilename] +  arg_array, stdout=subprocess.PIPE)
(stdo, _) = pc.communicate()
if pc.returncode != 0:
    sys.exit(pc.returncode)
open(ofilename, 'wb').write(stdo)
