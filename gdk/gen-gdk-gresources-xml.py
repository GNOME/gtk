#!/usr/bin/env python3
#
# Generate gdk.gresources.xml
#
# Usage: gen-gdk-gresources-xml SRCDIR_GDK [OUTPUT-FILE]

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

xml = '''<?xml version='1.0' encoding='UTF-8'?>
<gresources>
  <gresource prefix='/org/gtk/libgdk'>
    <file alias='cursor/alias'>cursors/alias_cursor.png</file>
    <file alias='cursor/all-resize'>cursors/all_resize_cursor.png</file>
    <file alias='cursor/all-scroll'>cursors/all_scroll_cursor.png</file>
    <file alias='cursor/cell'>cursors/cell_cursor.png</file>
    <file alias='cursor/col-resize'>cursors/col_resize_cursor.png</file>
    <file alias='cursor/context-menu'>cursors/context_menu_cursor.png</file>
    <file alias='cursor/copy'>cursors/copy_cursor.png</file>
    <file alias='cursor/crosshair'>cursors/crosshair_cursor.png</file>
    <file alias='cursor/default'>cursors/default_cursor.png</file>
    <file alias='cursor/e-resize'>cursors/e_resize_cursor.png</file>
    <file alias='cursor/ew-resize'>cursors/ew_resize_cursor.png</file>
    <file alias='cursor/grabbing'>cursors/grabbing_cursor.png</file>
    <file alias='cursor/grab'>cursors/grab_cursor.png</file>
    <file alias='cursor/help'>cursors/help_cursor.png</file>
    <file alias='cursor/move'>cursors/move_cursor.png</file>
    <file alias='cursor/ne-resize'>cursors/ne_resize_cursor.png</file>
    <file alias='cursor/nesw-resize'>cursors/nesw_resize_cursor.png</file>
    <file alias='cursor/no-drop'>cursors/no_drop_cursor.png</file>
    <file alias='cursor/not-allowed'>cursors/not_allowed_cursor.png</file>
    <file alias='cursor/n-resize'>cursors/n_resize_cursor.png</file>
    <file alias='cursor/nw-resize'>cursors/nw_resize_cursor.png</file>
    <file alias='cursor/nwse-resize'>cursors/nwse_resize_cursor.png</file>
    <file alias='cursor/pointer'>cursors/pointer_cursor.png</file>
    <file alias='cursor/progress'>cursors/progress_cursor.png</file>
    <file alias='cursor/row-resize'>cursors/row_resize_cursor.png</file>
    <file alias='cursor/se-resize'>cursors/se_resize_cursor.png</file>
    <file alias='cursor/s-resize'>cursors/s_resize_cursor.png</file>
    <file alias='cursor/sw-resize'>cursors/sw_resize_cursor.png</file>
    <file alias='cursor/text'>cursors/text_cursor.png</file>
    <file alias='cursor/vertical-text'>cursors/vertical_text_cursor.png</file>
    <file alias='cursor/wait'>cursors/wait_cursor.png</file>
    <file alias='cursor/w-resize'>cursors/w_resize_cursor.png</file>
    <file alias='cursor/zoom-in'>cursors/zoom_in_cursor.png</file>
    <file alias='cursor/zoom_out'>cursors/zoom_out_cursor.png</file>
'''

def get_files(subdir,extension):
  return sorted(filter(lambda x: x.endswith((extension)), os.listdir(os.path.join(srcdir,subdir))))

xml += '''
  </gresource>
</gresources>'''

if len(sys.argv) > 2:
  outfile = sys.argv[2]
  tmpfile = outfile + '~'
  with open(tmpfile, 'w') as f:
    f.write(xml)
  replace_if_changed(tmpfile, outfile)
else:
  print(xml)
