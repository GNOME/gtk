private_headers = [
]

images = [
]

configure_file(input: 'version.xml.in', output: 'version.xml', configuration: version_conf)

gnome.gtkdoc('gtk4',
             main_xml: 'gtk4-docs.xml',
             src_dir: [
	       join_paths(meson.source_root(), 'gtk'),
	       join_paths(meson.build_root(), 'gtk'),
	     ],
             dependencies: libgtk_dep,
             gobject_typesfile: 'gtk4.types',
             scan_args: [
               '--ignore-decorators=_GDK_EXTERN',
	       '--ignore-decorators=G_GNUC_WARN_UNUSED_RESULT',
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
