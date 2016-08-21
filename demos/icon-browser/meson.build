iconbrowser_sources = [
  'main.c',
  'iconbrowserapp.c',
  'iconbrowserwin.c',
  'iconstore.c'
]

iconbrowser_resources = gnome.compile_resources(
  'iconbrowser_resources',
  'iconbrowser.gresource.xml',
  source_dir: '.'
)

iconbrowser = executable(
  'gtk3-icon-browser',
  iconbrowser_sources,
  iconbrowser_resources,
  dependencies: libgtk_dep,
  link_with: libgtk,
  include_directories : confinc,
  gui_app: true
)
