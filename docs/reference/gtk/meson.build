# XXX: gi-docgen needs a deps file
expand_content_md_files = [
  'overview.md',
  'broadway.md',
  'osx.md',
  'wayland.md',
  'windows.md',
  'x11.md',
  'getting_started.md',
  'initialization.md',
  'resources.md',
  'building.md',
  'compiling.md',
  'running.md',
  'migrating-2to4.md',
  'migrating-3to4.md',
  'actions.md',
  'input-handling.md',
  'drawing-model.md',
  'css-overview.md',
  'css-properties.md',
  'section-accessibility.md',
  'section-text-widget.md',
  'section-tree-widget.md',
  'section-list-widget.md',
  'question_index.md',
  'visual_index.md'
]

gtk4_toml = configure_file(input: 'gtk4.toml.in', output: 'gtk4.toml', configuration: toml_conf)

if get_option('gtk_doc')
  custom_target('gtk4-doc',
    input: [ gtk4_toml, gtk_gir[0] ],
    output: 'gtk4',
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
    depends: [ gdk_gir[0], gsk_gir[0] ],
    depend_files: [ expand_content_md_files ],
    build_by_default: true,
    install: true,
    install_dir: docs_dir,
  )
endif

xsltproc = find_program('xsltproc', required: false)
if get_option('man-pages') and not xsltproc.found()
  error('No xsltproc found, but man pages were explicitly enabled')
endif

if get_option('man-pages') and xsltproc.found()
  xlstproc_flags = [
    '--nonet',
    '--stringparam', 'man.output.quietly', '1',
    '--stringparam', 'funcsynopsis.style', 'ansi',
    '--stringparam', 'man.th.extra1.suppress', '1',
    '--stringparam', 'man.authors.section.enabled', '0',
    '--stringparam', 'man.copyright.section.enabled', '0',
  ]

  man_files = [
    [ 'gtk4-broadwayd', '1', ],
    [ 'gtk4-builder-tool', '1', ],
    [ 'gtk4-encode-symbolic-svg', '1', ],
    [ 'gtk4-launch', '1', ],
    [ 'gtk4-query-settings', '1', ],
    [ 'gtk4-update-icon-cache', '1', ],
  ]

  if get_option('demos')
    man_files += [
      [ 'gtk4-demo', '1', ],
      [ 'gtk4-demo-application', '1', ],
      [ 'gtk4-widget-factory', '1', ],
      [ 'gtk4-icon-browser', '1', ],
    ]
  endif

  foreach man: man_files
    man_name = man.get(0)
    man_section = man.get(1, '1')
    custom_target('@0@.@1@'.format(man_name, man_section),
      input: '@0@.xml'.format(man_name),
      output: '@0@.@1@'.format(man_name, man_section),
      command: [
        xsltproc,
        xlstproc_flags,
        '-o', '@OUTPUT@',
        'http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl',
        '@INPUT@',
      ],
      install: true,
      install_dir: join_paths(get_option('mandir'), 'man@0@'.format(man_section)),
    )
  endforeach
endif
