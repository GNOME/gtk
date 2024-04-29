media_subdir = 'gtk-4.0/@0@/media'.format(gtk_binary_version)
media_install_dir = join_paths(get_option('libdir'), media_subdir)
media_backends = []

extra_c_args = ['-DGTK_COMPILATION']
extra_c_args += common_cflags

if gstplayer_dep.found() and gstgl_dep.found() and gstallocators_dep.found()
  media_backends += 'gstreamer'
  cdata.set('HAVE_GSTREAMER', 1)
  media_gst_deps = [ libm, libgtk_dep, gstplayer_dep, gstgl_dep, gstallocators_dep ]

  shared_module('media-gstreamer',
    sources: [
      'gtkgstmediafile.c',
      'gtkgstpaintable.c',
      'gtkgstsink.c',
    ],
    c_args: extra_c_args,
    dependencies: media_gst_deps,
    name_suffix: module_suffix,
    install_dir: media_install_dir,
    install: true,
  )
endif

if media_backends.length() > 0
  gio_module_dirs += media_install_dir
endif
