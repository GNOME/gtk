all_media_backends = [
  'ffmpeg',
  'gstreamer'
]

enabled_media_backends = get_option('media').split(',')

if enabled_media_backends.contains('none')
  media_backends = []
elif enabled_media_backends.contains('all')
  media_backends = all_media_backends
else
  media_backends = []
  foreach b: enabled_media_backends
    if all_media_backends.contains(b)
      media_backends += b
    else
      error('No media backend named "@0@" exists.'.format (b))
    endif
  endforeach
endif

media_subdir = 'gtk-4.0/@0@/media'.format(gtk_binary_version)
media_install_dir = join_paths(get_option('libdir'), media_subdir)

extra_c_args = ['-DGTK_COMPILATION']

# Detect and set symbol visibility
if cc.get_id() == 'msvc'
  extra_c_args += ['-D_GLIB_EXTERN=__declspec (dllexport) extern']
endif

if media_backends.contains('ffmpeg')
  libavfilter_dep = dependency('libavfilter', version: '>= 6.47.100', required: true)
  libavformat_dep = dependency('libavformat', version: '>= 57.41.100', required: true)
  libavcodec_dep = dependency('libavcodec', version: '>= 57.48.101', required: true)
  libavutil_dep = dependency('libavutil', version: '>= 55.28.100', required: true)
  libswscale_dep = dependency('libswscale', version: '>= 4.6.100', required: true)
  ffmpeg_deps = [libavfilter_dep, libavformat_dep, libavcodec_dep, libavutil_dep, libswscale_dep]
  cdata.set('HAVE_FFMPEG', 1)

  shared_module('media-ffmpeg',
                'gtkffmediafile.c',
                c_args: extra_c_args,
                dependencies: [ libgtk_dep, ffmpeg_deps ],
                install_dir: media_install_dir,
                install : true)
endif

if media_backends.contains('gstreamer')
  gstplayer_dep = dependency('gstreamer-player-1.0', version: '>= 1.12.3', required: true)
  cdata.set('HAVE_GSTREAMER', 1)

  shared_module('media-gstreamer',
                'gtkgstmediafile.c',
                'gtkgstpaintable.c',
                'gtkgstsink.c',
                c_args: extra_c_args,
                dependencies: [ libgtk_dep, gstplayer_dep ],
                install_dir: media_install_dir,
                install : true)
endif
