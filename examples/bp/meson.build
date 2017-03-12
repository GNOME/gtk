bp_resources = gnome.compile_resources('bloatpad_resources',
  'bloatpad.gresources.xml',
  source_dir: '.')

executable('bloatpad', 'bloatpad.c', bp_resources, dependencies: libgtk_dep)
