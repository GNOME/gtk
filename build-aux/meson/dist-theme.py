#!/usr/bin/env python3

import os
from pathlib import PurePath
import subprocess

stylesheets = [ 'gtk/theme/Adwaita/Adwaita.css',
                'gtk/theme/Adwaita/Adwaita-dark.css',
                'gtk/theme/HighContrast/HighContrast.css',
                'gtk/theme/HighContrast/HighContrast-dark.css' ]

sourceroot = os.environ.get('MESON_SOURCE_ROOT')
distroot = os.environ.get('MESON_DIST_ROOT')

for stylesheet in stylesheets:
  stylesheet_path = PurePath(stylesheet)
  src = PurePath(sourceroot, stylesheet_path.with_suffix('.scss'))
  dst = PurePath(distroot, stylesheet_path)
  subprocess.call(['sassc', '-a', '-M', '-t', 'compact', src, dst])
