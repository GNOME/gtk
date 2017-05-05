private_headers = [
]

images = [
]

configure_file(input: 'version.xml.in', output: 'version.xml', configuration: version_conf)

types_conf = configuration_data()
if os_win32
  types_conf.set('DISABLE_ON_W32', '%')
else
  types_conf.set('DISABLE_ON_W32', '')
endif

if os_darwin
  types_conf.set('DISABLE_ON_QUARTZ', '%')
else
  types_conf.set('DISABLE_ON_QUARTZ', '')
endif

configure_file(input: 'gtk4.types.in', output: 'gtk4.types', configuration: types_conf)

gnome.gtkdoc('gtk4',
             main_xml: 'gtk4-docs.xml',
             src_dir: [
	       join_paths(meson.source_root(), 'gtk'),
	       join_paths(meson.build_root(), 'gtk'),
	     ],
             dependencies: libgtk_dep,
             gobject_typesfile: 'gtk4.types',
             scan_args: [
               '--ignore-decorators=_GDK_EXTERN|G_GNUC_WARN_UNUSED_RESULT',
               '--ignore-headers=' + ' '.join(private_headers),
             ],
             fixxref_args: [
               '--html-dir=@0@'.format(docpath),
               '--extra-dir=@0@'.format(join_paths(glib_docpath, 'glib')),
               '--extra-dir=@0@'.format(join_paths(glib_docpath, 'gobject')),
               '--extra-dir=@0@'.format(join_paths(glib_docpath, 'gio')),
               '--extra-dir=@0@'.format(cairo_docpath),
               '--extra-dir=@0@'.format(gdkpixbuf_docpath),
	       '--extra-dir=../gdk',
	       '--extra-dir=../gsk',
             ],
             html_assets: images,
             install: true)

xsltproc = find_program('xsltproc', required: false)
if get_option('enable-man-pages') and not xsltproc.found()
  error('No xsltproc found, but man pages were explicitly enabled')
endif

if get_option('enable-man-pages') and xsltproc.found()
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
    [ 'gtk4-demo', '1', ],
    [ 'gtk4-demo-application', '1', ],
    [ 'gtk4-encode-symbolic-svg', '1', ],
    [ 'gtk4-icon-browser', '1', ],
    [ 'gtk4-launch', '1', ],
    [ 'gtk4-query-immodules', '1', ],
    [ 'gtk4-query-settings', '1', ],
    [ 'gtk4-update-icon-cache', '1', ],
    [ 'gtk4-widget-factory', '1', ],
  ]

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
                  install_dir: join_paths(get_option('mandir'), 'man@0@'.format(man_section)))
  endforeach
endif
