expand_content_files = [
  'boxed.md',
  'concepts.md',
  'enum-types.md',
  'floating-refs.md',
  'gvalue.md',
  'tutorial.md',
]

gobject_gir = meson.current_source_dir() / 'GObject-2.0.gir'
gobject_toml = configure_file(input: 'gobject.toml.in', output: 'gobject.toml', configuration: toml_conf)

custom_target('gobject-doc',
  input: [ gobject_toml, gobject_gir ],
  output: 'gobject',
  command: [
    gidocgen,
    'generate',
    gidocgen_common_args,
    '--config=@INPUT0@',
    '--output-dir=@OUTPUT@',
    '--content-dir=@0@'.format(meson.current_source_dir()),
    '--add-include-path=@0@'.format(meson.current_source_dir() / '../glib'),
    '@INPUT1@',
  ],
  build_by_default: true,
  depend_files: expand_content_files,
)
