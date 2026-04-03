#!/usr/bin/env python3
#
# Generate gsk.resources.xml
#
# Usage: gen-gsk-gresources-xml OUTPUT-FILE [INPUT-FILE1] [INPUT-FILE2] ...

import os, sys
import filecmp

def replace_if_changed(new, old):
  '''
  Compare contents and only replace if changed to avoid triggering a rebuild.
  '''
  try:
    changed = not filecmp.cmp(new, old, shallow=False)
  except FileNotFoundError:
    changed = True
  if changed:
    os.replace(new, old)
  else:
    os.remove(new)

sources = []
gl_source_shaders = []

for f in sys.argv[2:]:
  if f.endswith('.glsl'):
    if f.find('generated') > -1:
      gl_source_shaders.append(f)
    else:
      sources.append(f)
  else:
    raise Exception(f"No idea what XML to generate for {f}")

xml = '''<?xml version='1.0' encoding='UTF-8'?>
<gresources>
  <gresource prefix='/org/gtk/libgsk'>

'''

for f in sources:
  xml += '    <file alias=\'shaders/sources/{0}\'>gpu/shaders/{0}</file>\n'.format(os.path.basename(f))

xml += '\n'

for f in gl_source_shaders:
  xml += '    <file alias=\'shaders/gl/{0}\'>gpu/shaders/{0}</file>\n'.format(os.path.basename(f))

xml += '''
  </gresource>
</gresources>'''

if len(sys.argv) > 1 and sys.argv[1] != '-':
  outfile = sys.argv[1]
  tmpfile = outfile + '~'
  with open(tmpfile, 'w') as f:
    f.write(xml)
  replace_if_changed(tmpfile, outfile)
else:
  print(xml)
