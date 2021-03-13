gsk4_toml = configure_file(input: 'gsk4.toml.in', output: 'gsk4.toml', configuration: toml_conf)

if get_option('gtk_doc')
  custom_target('gsk4-doc',
    input: [ gsk4_toml, gsk_gir[0] ],
    output: 'gsk4',
    command: [
      gidocgen,
      'generate',
      '--quiet',
      '--add-include-path=@0@'.format(meson.current_build_dir() / '../../../gtk'),
      '--config=@INPUT0@',
      '--output-dir=@OUTPUT@',
      '--no-namespace-dir',
      '--content-dir=@0@'.format(meson.current_source_dir()),
      '@INPUT1@',
    ],
    depends: [ gdk_gir[0] ],
    depend_files: [ expand_content_md_files ],
    build_by_default: true,
    install: true,
    install_dir: docs_dir,
  )
endif
