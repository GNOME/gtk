
bp_resources = gnome.compile_resources(
  'bloatpad resources',
  'bloatpad.gresources.xml',
  source_dir: '.'
)

bloatpad = executable(
  'bloatpad',
  'bloatpad.c',
  bp_resources,
  dependencies: libgtk_dep
)
