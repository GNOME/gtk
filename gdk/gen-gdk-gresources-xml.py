#!/usr/bin/env python3
#
# Generate gdk.gresources.xml
#
# Usage: gen-gdk-gresources-xml SRCDIR_GDK [OUTPUT-FILE]

import os, sys

srcdir = sys.argv[1]

xml = '''<?xml version='1.0' encoding='UTF-8'?>
<gresources>
  <gresource prefix='/org/gtk/libgdk'>

'''

def get_files(subdir,extension):
  return sorted(filter(lambda x: x.endswith((extension)), os.listdir(os.path.join(srcdir,subdir))))

for f in get_files('resources/glsl', '.glsl'):
  xml += '    <file alias=\'glsl/{0}\'>resources/glsl/{0}</file>\n'.format(f)

xml += '''
  </gresource>
</gresources>'''

if len(sys.argv) > 2:
  outfile = sys.argv[2]
  f = open(outfile, 'w', encoding='utf-8')
  f.write(xml)
  f.close()
else:
  print(xml)
