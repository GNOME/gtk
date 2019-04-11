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
  include_directories : confinc,
  install: true
)

# desktop file
install_data('gtk3-icon-browser.desktop', install_dir: gtk_applicationsdir)
