expand_content_files = [
]

atspi_gir = meson.current_source_dir() / 'Atspi-2.0.gir'
atspi_toml = configure_file(input: 'atspi2.toml.in', output: 'atspi2.toml', configuration: toml_conf)

custom_target('atspi2-doc',
  input: [ atspi_toml, atspi_gir ],
  output: 'atspi2',
  command: [
    gidocgen,
    'generate',
    '--quiet',
    '--config=@INPUT0@',
    '--output-dir=@OUTPUT@',
    '--no-namespace-dir',
    '--content-dir=@0@'.format(meson.current_source_dir()),
    '@INPUT1@',
  ],
  build_by_default: true,
  depend_files: expand_content_files,
)
