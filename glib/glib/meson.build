expand_content_files = [
  'building.md',
  'character-set.md',
  'compiling.md',
  'cross-compiling.md',
  'error-reporting.md',
  'gvariant-format-strings.md',
  'gvariant-text.md',
  'i18n.md',
  'logging.md',
  'main-loop.md',
  'memory-slices.md',
  'reference-counting.md',
  'running.md',
  'testing.md',
  'threads.md',
]

glib_gir = meson.current_source_dir() / 'GLib-2.0.gir'
glib_toml = configure_file(input: 'glib.toml.in', output: 'glib.toml', configuration: toml_conf)

custom_target('glib-doc',
  input: [ glib_toml, glib_gir ],
  output: 'glib',
  command: [
    gidocgen,
    'generate',
    gidocgen_common_args,
    '--config=@INPUT0@',
    '--output-dir=@OUTPUT@',
    '--content-dir=@0@'.format(meson.current_source_dir()),
    '@INPUT1@',
  ],
  build_by_default: true,
  depend_files: expand_content_files,
)
