ottie_editor_sources = [
  'main.c',
  'ottie-editor-application.c',
  'ottie-editor-window.c',
]

ottie_editor_resources = gnome.compile_resources('ottie_editor_resources',
  'ottie-editor.gresource.xml',
  source_dir: '.',
)

executable('ottie-editor',
  sources: [ottie_editor_sources, ottie_editor_resources],
  dependencies: [libgtk_css_dep, libgdk_dep, libgsk_dep, libgtk_static_dep, libottie_dep],
  include_directories: confinc,
  c_args: [
    '-DGTK_COMPILATION',
    '-DG_LOG_DOMAIN="OttieEditor"',
  ] + common_cflags,
  gui_app: true,
  link_with: [libgtk_static, libgtk_css, libgdk, libgsk, libottie],
  link_args: common_ldflags,
  install: false,
)

# icons, don't install them until we decide to install ottie-editor
#icontheme_dir = join_paths(gtk_datadir, 'icons/hicolor')

#foreach size: ['scalable', 'symbolic']
#  install_subdir('data/' + size, install_dir: icontheme_dir)
#endforeach
