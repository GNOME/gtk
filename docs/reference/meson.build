if get_option('gtk_doc')
    glib_prefix = dependency('glib-2.0').get_variable(pkgconfig: 'prefix')
    glib_docpath = join_paths(glib_prefix, 'share', 'gtk-doc', 'html')

    cairo_prefix = dependency('cairo-gobject').get_variable(pkgconfig: 'prefix')
    cairo_docpath = join_paths(cairo_prefix, 'share', 'gtk-doc', 'html', 'cairo')

    gdkpixbuf_prefix = dependency('gdk-pixbuf-2.0').get_variable(pkgconfig: 'prefix')
    gdkpixbuf_docpath = join_paths(gdkpixbuf_prefix, 'share', 'gtk-doc', 'html', 'gdk-pixbuf')

    docpath = join_paths(gtk_datadir, 'gtk-doc', 'html')

    version_conf = configuration_data()
    version_conf.set('GTK_VERSION', meson.project_version())

    src_dir_conf = configuration_data()
    src_dir_conf.set('abs_top_srcdir', meson.source_root())
endif

subdir('gdk')
subdir('gtk')
subdir('libgail-util')
