icon_editor_sources = [
  'border-paintable.c',
  'color-editor.c',
  'color-paintable.c',
  'icon-editor-application.c',
  'icon-editor-window.c',
  'fontify.c',
  'main.c',
  'mini-graph.c',
  'paintable-editor.c',
  'path-editor.c',
  'path-paintable.c',
  'range-editor.c',
  'state-editor.c',
]

icon_editor_resources = gnome.compile_resources('icon_editor_resources',
  'icon-editor.gresource.xml',
  source_dir: meson.current_source_dir(),
)


executable('gtk4-icon-editor',
  sources: [icon_editor_sources, icon_editor_resources],
  c_args: common_cflags,
  dependencies: [ libgtk_dep, gmodule_headers_dep, profile_conf_h ],
  include_directories: confinc,
  win_subsystem: 'windows',
  link_args: extra_demo_ldflags,
  install: true,
)

install_data('org.gtk.Shaper.desktop', install_dir: gtk_applicationsdir)

icontheme_dir = join_paths(gtk_datadir, 'icons/hicolor')

install_data('org.gtk.Shaper.svg', install_dir: join_paths(icontheme_dir, 'scalable/apps'))
install_data('org.gtk.Shaper.Devel.svg', install_dir: join_paths(icontheme_dir, 'scalable/apps'))
install_data('org.gtk.Shaper-symbolic.svg', install_dir: join_paths(icontheme_dir, 'symbolic/apps'))
