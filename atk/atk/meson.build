expand_content_files = [
]

atk_gir = meson.current_source_dir() / 'Atk-1.0.gir'
atk_toml = configure_file(input: 'atk.toml.in', output: 'atk.toml', configuration: toml_conf)

custom_target('atk-doc',
  input: [ atk_toml, atk_gir ],
  output: 'atk',
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
