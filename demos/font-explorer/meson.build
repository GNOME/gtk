fontexplorer_sources = [
  'main.c',
  'fontexplorerapp.c',
  'fontexplorerwin.c',
  'fontcontrols.c',
  'samplechooser.c',
  'fontcolors.c',
  'fontfeatures.c',
  'fontvariations.c',
  'fontview.c',
  'rangeedit.c',
  'language-names.c',
]

fontexplorer_resources = gnome.compile_resources('fontexplorer_resources',
  'fontexplorer.gresource.xml',
  source_dir: '.',
)

executable('gtk4-font-explorer',
  sources: [fontexplorer_sources, fontexplorer_resources],
  c_args: common_cflags,
  dependencies: [ libgtk_dep, demo_conf_h ],
  include_directories: confinc,
  link_args: extra_demo_ldflags,
  install: true,
)
