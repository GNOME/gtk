media_subdir = 'gtk-4.0/@0@/media'.format(gtk_binary_version)
media_install_dir = join_paths(get_option('libdir'), media_subdir)

if ffmpeg_enabled
  shared_module('media-ffmpeg',
                'gtkffmediafile.c',
                c_args: [
                  '-DGTK_COMPILATION'
                ],
                dependencies: [ libgtk_dep, ffmpeg_deps ],
                install_dir: media_install_dir,
                install : true)
endif

if gstreamer_enabled
  shared_module('media-gstreamer',
                'gtkgstmediafile.c',
                'gtkgstpaintable.c',
                'gtkgstsink.c',
                c_args: [
                  '-DGTK_COMPILATION'
                ],
                dependencies: [ libgtk_dep, gstreamer_deps ],
                install_dir: media_install_dir,
                install : true)
endif
