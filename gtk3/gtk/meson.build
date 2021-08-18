expand_content_files = [
  'broadway.md',
  'changes.md',
  'css-overview.md',
  'drawing-model.md',
  'input-handling.md',
  'key-bindings.md',
  'macos.md',
  'migrating-2to3.md',
  'migrating-libunique.md',
  'migrating-smclient.md',
  'migrating-themes.md',
  'running.md',
  'text-widget-overview.md',
  'tree-widget-overview.md',
  'treeview-tutorial.md',
  'wayland.md',
  'windows.md',
  'x11.md',
]

gtk_gir = meson.current_source_dir() / 'Gtk-3.0.gir'
gtk_toml = configure_file(input: 'gtk3.toml.in', output: 'gtk3.toml', configuration: toml_conf)

custom_target('gtk3-doc',
  input: [ gtk_toml, gtk_gir ],
  output: 'gtk3',
  command: [
    gidocgen,
    'generate',
    '--quiet',
    '--config=@INPUT0@',
    '--output-dir=@OUTPUT@',
    '--no-namespace-dir',
    '--content-dir=@0@'.format(meson.current_source_dir()),
    '--add-include-path=@0@'.format(meson.current_source_dir() / '../gdk'),
    '@INPUT1@',
  ],
  build_by_default: true,
  depend_files: expand_content_files,
)
