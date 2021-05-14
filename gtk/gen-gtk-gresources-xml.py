#!/usr/bin/env python3
#
# Generate gtk.gresources.xml
#
# Usage: gen-gtk-gresources-xml SRCDIR_GTK [OUTPUT-FILE]

import os, sys

srcdir = sys.argv[1]

xml = '''<?xml version='1.0' encoding='UTF-8'?>
<gresources>
  <gresource prefix='/org/gtk/libgtk'>
'''

def get_files(subdir,extension):
  return sorted(filter(lambda x: x.endswith((extension)), os.listdir(os.path.join(srcdir,subdir))))

xml += '''
    <file>theme/Adwaita/gtk.css</file>
    <file>theme/Adwaita/gtk-dark.css</file>
    <file>theme/Adwaita/gtk-contained.css</file>
    <file>theme/Adwaita/gtk-contained-dark.css</file>
'''

for f in get_files('theme/Adwaita/assets', '.png'):
  xml += '    <file preprocess=\'to-pixdata\'>theme/Adwaita/assets/{0}</file>\n'.format(f)

xml += '\n'

for f in get_files('theme/Adwaita/assets', '.svg'):
  xml += '    <file>theme/Adwaita/assets/{0}</file>\n'.format(f)

xml += '''
    <file>theme/HighContrast/gtk.css</file>
    <file alias='theme/HighContrastInverse/gtk.css'>theme/HighContrast/gtk-inverse.css</file>
    <file>theme/HighContrast/gtk-contained.css</file>
    <file>theme/HighContrast/gtk-contained-inverse.css</file>
'''

for f in get_files('theme/HighContrast/assets', '.png'):
  xml += '    <file preprocess=\'to-pixdata\'>theme/HighContrast/assets/{0}</file>\n'.format(f)

xml += '\n'

for f in get_files('theme/HighContrast/assets', '.svg'):
  xml += '    <file>theme/HighContrast/assets/{0}</file>\n'.format(f)

xml += '''
    <file>theme/win32/gtk-win32-base.css</file>
    <file>theme/win32/gtk.css</file>
'''

for f in get_files('cursor', '.png'):
  xml += '    <file>cursor/{0}</file>\n'.format(f)

for f in get_files('gesture', '.symbolic.png'):
  xml += '    <file alias=\'icons/64x64/actions/{0}\'>gesture/{0}</file>\n'.format(f)

xml += '\n'

for f in get_files('ui', '.ui'):
  xml += '    <file preprocess=\'xml-stripblanks\'>ui/{0}</file>\n'.format(f)

xml += '\n'

for s in ['16x16', '22x22', '24x24', '32x32', '48x48']:
  for c in ['actions', 'status', 'categories']:
    icons_dir = 'icons/{0}/{1}'.format(s,c)
    if os.path.exists(os.path.join(srcdir,icons_dir)):
      for f in get_files(icons_dir, '.png'):
        xml += '    <file>icons/{0}/{1}/{2}</file>\n'.format(s,c,f)

for f in get_files('inspector', '.ui'):
  xml += '    <file compressed=\'true\' preprocess=\'xml-stripblanks\'>inspector/{0}</file>\n'.format(f)

xml += '''
    <file>inspector/logo.png</file>
    <file>emoji/en.data</file>
  </gresource>
</gresources>'''

if len(sys.argv) > 2:
  outfile = sys.argv[2]
  f = open(outfile, 'w', encoding='utf-8')
  f.write(xml)
  f.close()
else:
  print(xml)
