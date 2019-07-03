build_docs = get_option('gtk_doc')
if build_docs
  gtkdoc_scangobj = find_program('gtkdoc-scangobj')
  gtkdoc_ver = run_command(gtkdoc_scangobj, '--version').stdout().strip()
  if gtkdoc_ver.version_compare('<1.30')
    warning('GTK-Doc version is too old for GTK4')
    build_docs = false
  endif
endif

if build_docs
  glib_prefix = dependency('glib-2.0').get_pkgconfig_variable('prefix')
  glib_docpath = join_paths(glib_prefix, 'share', 'gtk-doc', 'html')

  cairo_prefix = dependency('cairo-gobject').get_pkgconfig_variable('prefix')
  cairo_docpath = join_paths(cairo_prefix, 'share', 'gtk-doc', 'html', 'cairo')

  gdkpixbuf_prefix = dependency('gdk-pixbuf-2.0').get_pkgconfig_variable('prefix')
  gdkpixbuf_docpath = join_paths(gdkpixbuf_prefix, 'share', 'gtk-doc', 'html', 'gdk-pixbuf')

  docpath = join_paths(gtk_datadir, 'gtk-doc', 'html')

  version_conf = configuration_data()
  version_conf.set('GTK_VERSION', meson.project_version())

  src_dir_conf = configuration_data()
  src_dir_conf.set('SRC_DIR', meson.source_root())

  subdir('gdk')
  subdir('gsk')
  subdir('gtk')
endif
