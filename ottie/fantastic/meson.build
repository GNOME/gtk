fantastic_sources = [
  'main.c',
  'fantasticapplication.c',
  'fantasticobserver.c',
  'fantasticwindow.c',
]

fantastic_resources = gnome.compile_resources('fantastic_resources',
  'fantastic.gresource.xml',
  source_dir: '.',
)

executable('fantastic',
  sources: [fantastic_sources, fantastic_resources],
  dependencies: [libgtk_css_dep, libgdk_dep, libgsk_dep, libgtk_static_dep, libottie_dep],
  include_directories: confinc,
  c_args: [
    '-DGTK_COMPILATION',
    '-DG_LOG_DOMAIN="Fantastic"',
  ] + common_cflags,
  gui_app: true,
  link_with: [libgtk_static, libgtk_css, libgdk, libgsk, libottie],
  link_args: common_ldflags,
  install: false,
)

# icons, don't install them until we decide to install fantastic
#icontheme_dir = join_paths(gtk_datadir, 'icons/hicolor')

#foreach size: ['scalable', 'symbolic']
#  install_subdir('data/' + size, install_dir: icontheme_dir)
#endforeach
