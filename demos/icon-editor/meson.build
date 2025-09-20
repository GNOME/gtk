icon_editor_sources = [
  'border-paintable.c',
  'icon-editor-application.c',
  'icon-editor-window.c',
  'fontify.c',
  'main.c',
  'paintable-editor.c',
  'path-editor.c',
  'path-paintable.c',
  'path-paintable-parser.c',
  'range-editor.c',
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

install_data('org.gtk.IconEditor.desktop', install_dir: gtk_applicationsdir)
