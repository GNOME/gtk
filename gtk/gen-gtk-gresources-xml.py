#!/usr/bin/env python3
#
# Generate gtk.gresources.xml
#
# Usage: gen-gtk-gresources-xml SRCDIR_GTK [OUTPUT-FILE]

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

srcdir = sys.argv[1]
endian = sys.argv[2]

xml = '''<?xml version='1.0' encoding='UTF-8'?>
<gresources>
  <gresource prefix='/org/gtk/libgtk'>
'''

def get_files(subdir,extension):
  return sorted(filter(lambda x: x.endswith((extension)), os.listdir(os.path.join(srcdir,subdir))))

xml += '''
    <file>theme/Empty/gtk.css</file>

    <file>theme/Default/gtk.css</file>
    <file>theme/Default/Default-light.css</file>
    <file>theme/Default/Default-dark.css</file>
    <file>theme/Default/Default-hc.css</file>
    <file>theme/Default/Default-hc-dark.css</file>
'''

for f in get_files('theme/Default/assets', '.png'):
  xml += '    <file>theme/Default/assets/{0}</file>\n'.format(f)

xml += '\n'

for f in get_files('theme/Default/assets', '.svg'):
  xml += '    <file preprocess=\'xml-stripblanks\'>theme/Default/assets/{0}</file>\n'.format(f)

for f in get_files('theme/Default/assets-hc', '.png'):
  xml += '    <file>theme/Default/assets-hc/{0}</file>\n'.format(f)

xml += '\n'

for f in get_files('theme/Default/assets-hc', '.svg'):
  xml += '    <file preprocess=\'xml-stripblanks\'>theme/Default/assets-hc/{0}</file>\n'.format(f)

for f in get_files('ui', '.ui'):
  xml += '    <file>ui/{0}</file>\n'.format(f)

for f in get_files('print/ui', '.ui'):
  xml += '    <file>print/ui/{0}</file>\n'.format(f)

xml += '\n'

xml += '''
    <file compressed="true">icons/hicolor.index.theme</file>
'''

xml += '    <file>icons/16x16/status/image-missing.png</file>\n'

for f in get_files('icons', '.svg'):
  xml += '    <file preprocess=\'xml-stripblanks\'>icons/{0}</file>\n'.format(f)

for f in get_files('icons', '.gpa'):
  xml += '    <file preprocess=\'xml-stripblanks\'>icons/{0}</file>\n'.format(f)

for f in get_files('inspector', '.ui'):
  xml += '    <file preprocess=\'xml-stripblanks\'>inspector/{0}</file>\n'.format(f)

xml += '''
    <file>inspector/inspector.css</file>
    <file>emoji/en.data</file>
    <file alias="compose/sequences">compose/sequences-{0}-endian</file>
    <file>compose/chars</file>
  </gresource>
</gresources>'''.format(endian)

if len(sys.argv) > 3:
  outfile = sys.argv[3]
  tmpfile = outfile + '~'
  with open(tmpfile, 'w') as f:
    f.write(xml)
  replace_if_changed(tmpfile, outfile)
else:
  print(xml)
