#!/usr/bin/env python3
#
# Generate gsk.resources.xml
#
# Usage: gen-gsk-gresources-xml OUTPUT-FILE [INPUT-FILE1] [INPUT-FILE2] ...

import os, sys

source_shaders = []
vulkan_compiled_shaders = []
vulkan_shaders = []

for f in sys.argv[2:]:
  if f.endswith('.glsl'):
    source_shaders.append(f)
  elif f.endswith('.spv'):
    vulkan_compiled_shaders.append(f)
  elif f.endswith('.frag') or f.endswith('.vert'):
    vulkan_shaders.append(f)
  else:
    sys.exit(-1) # FIXME: error message

xml = '''<?xml version='1.0' encoding='UTF-8'?>
<gresources>
  <gresource prefix='/org/gtk/libgsk'>

'''

for f in source_shaders:
  xml += '    <file alias=\'glsl/{0}\'>resources/glsl/{0}</file>\n'.format(os.path.basename(f))

xml += '\n'

for f in vulkan_compiled_shaders:
  xml += '    <file alias=\'vulkan/{0}\'>resources/vulkan/{0}</file>\n'.format(os.path.basename(f))

xml += '\n'

for f in vulkan_shaders:
  xml += '    <file alias=\'vulkan/{0}\'>resources/vulkan/{0}</file>\n'.format(os.path.basename(f))

xml += '''
  </gresource>
</gresources>'''

if len(sys.argv) > 1 and sys.argv[1] != '-':
  outfile = sys.argv[1]
  f = open(outfile, 'w')
  f.write(xml)
  f.close()
else:
  print(xml)
