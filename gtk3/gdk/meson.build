expand_content_files = [
]

gdk_gir = meson.current_source_dir() / 'Gdk-3.0.gir'
gdk_toml = configure_file(input: 'gdk3.toml.in', output: 'gdk3.toml', configuration: toml_conf)

custom_target('gdk3-doc',
  input: [ gdk_toml, gdk_gir ],
  output: 'gdk3',
  command: [
    gidocgen,
    'generate',
    gidocgen_common_args,
    '--config=@INPUT0@',
    '--output-dir=@OUTPUT@',
    '--content-dir=@0@'.format(meson.current_source_dir()),
    '--add-include-path=@0@'.format(meson.current_source_dir()),
    '@INPUT1@',
  ],
  build_by_default: true,
  depend_files: expand_content_files,
)

gdkx11_gir = meson.current_source_dir() / 'GdkX11-3.0.gir'
gdkx11_toml = configure_file(input: 'gdk3-x11.toml.in', output: 'gdk3-x11.toml', configuration: toml_conf)

custom_target('gdk3-x11-doc',
  input: [ gdkx11_toml, gdkx11_gir ],
  output: 'gdk3-x11',
  command: [
    gidocgen,
    'generate',
    gidocgen_common_args,
    '--config=@INPUT0@',
    '--output-dir=@OUTPUT@',
    '--content-dir=@0@'.format(meson.current_source_dir()),
    '--add-include-path=@0@'.format(meson.current_source_dir()),
    '@INPUT1@',
  ],
  build_by_default: true,
  depend_files: expand_content_files,
)
