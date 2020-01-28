if get_option('gtk_doc')
  if not meson.version().version_compare('>=0.52.0')
    error('Building the GTK documentation requires Meson 0.52.0')
  endif

  # Use gtk-doc as a sub-project if the version isn't new enough
  dependency('gtk-doc', version: '>=1.32',
    fallback: ['gtk-doc', 'dummy_dep'],
    default_options: ['tests=false'])

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
endif

subdir('gdk')
subdir('gsk')
subdir('gtk')
