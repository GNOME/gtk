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
  xml += '    <file>theme/Adwaita/assets/{0}</file>\n'.format(f)

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
  xml += '    <file>theme/HighContrast/assets/{0}</file>\n'.format(f)

xml += '\n'

for f in get_files('theme/HighContrast/assets', '.svg'):
  xml += '    <file>theme/HighContrast/assets/{0}</file>\n'.format(f)

for f in get_files('gesture', '.symbolic.png'):
  xml += '    <file alias=\'icons/64x64/actions/{0}\'>gesture/{0}</file>\n'.format(f)

xml += '\n'

for f in get_files('ui', '.ui'):
  xml += '    <file preprocess=\'xml-stripblanks\'>ui/{0}</file>\n'.format(f)

xml += '\n'

for s in ['16x16', '24x24', '32x32', '48x48', 'scalable']:
  for c in ['actions', 'categories', 'emblems', 'emotes', 'devices', 'mimetypes', 'places', 'status']:
    icons_dir = 'icons/{0}/{1}'.format(s,c)
    if os.path.exists(os.path.join(srcdir,icons_dir)):
      for f in get_files(icons_dir, '.png'):
        xml += '    <file>icons/{0}/{1}/{2}</file>\n'.format(s,c,f)
      for f in get_files(icons_dir, '.svg'):
        xml += '    <file>icons/{0}/{1}/{2}</file>\n'.format(s,c,f)

for f in get_files('inspector', '.ui'):
  xml += '    <file preprocess=\'xml-stripblanks\'>inspector/{0}</file>\n'.format(f)

xml += '''
    <file>inspector/logo.png</file>
    <file>inspector/inspector.css</file>
    <file>emoji/emoji.data</file>
  </gresource>
</gresources>'''

if len(sys.argv) > 2:
  outfile = sys.argv[2]
  f = open(outfile, 'w')
  f.write(xml)
  f.close()
else:
  print(xml)
