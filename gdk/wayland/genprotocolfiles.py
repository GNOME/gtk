#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
import re
import shutil
import subprocess

scanner  = sys.argv[1]
in_file  = sys.argv[2]
out_file = sys.argv[3]
#TODO: We can infer this optinon from the name of the output file!
option   = sys.argv[4]



pc = subprocess.Popen([scanner, option , in_file , out_file], stdout=subprocess.PIPE)
(stdo, _) = pc.communicate()
if pc.returncode != 0:
    sys.exit(pc.returncode)

# Now read the generated file again and remove all WL_EXPORTs
content = ""
with open(out_file, 'r') as content_file:
    content = content_file.read()

content = content.replace("WL_EXPORT", "")
ofile = open(out_file, 'w')
ofile.write(content)
ofile.close()



# unstable = False

# if "unstable" in out_file:
    # unstable = True


# if out_file.endswith("-protocol.c"):
    # print("protocol source")
# elif out_file.endswith("-client-protocol.h"):
    # print("client protocol header")
# elif out_file.endswith("-server-protocol.h"):
    # print("server protocol header")
# else:
    # print("ERROR: '",out_file,"' is not a valid output file")
