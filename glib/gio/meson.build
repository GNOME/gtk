expand_content_files = [
  'migrating-gconf.md',
  'migrating-gdbus.md',
  'migrating-gnome-vfs.md',
  'overview.md',
]

gio_gir = meson.current_source_dir() / 'Gio-2.0.gir'
gio_toml = configure_file(input: 'gio.toml.in', output: 'gio.toml', configuration: toml_conf)

custom_target('gio-doc',
  input: [ gio_toml, gio_gir ],
  output: 'gio',
  command: [
    gidocgen,
    'generate',
    gidocgen_common_args,
    '--config=@INPUT0@',
    '--output-dir=@OUTPUT@',
    '--content-dir=@0@'.format(meson.current_source_dir()),
    '--add-include-path=@0@'.format(meson.current_source_dir() / '../glib'),
    '--add-include-path=@0@'.format(meson.current_source_dir() / '../gobject'),
    '@INPUT1@',
  ],
  build_by_default: true,
  depend_files: expand_content_files,
)
