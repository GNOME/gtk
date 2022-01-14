media_subdir = 'gtk-4.0/@0@/media'.format(gtk_binary_version)
media_install_dir = join_paths(get_option('libdir'), media_subdir)
media_backends = []

extra_c_args = [
  '-DGTK_COMPILATION',
  '-D_GLIB_EXTERN=@0@'.format(visibility_define),
]

extra_c_args += common_cflags

ffmpeg_opt = get_option('media-ffmpeg')
ffmpeg_versions = {
  'libavfilter': '>= 6.47.100',
  'libavformat': '>= 57.41.100',
  'libavcodec': '>= 57.48.101',
  'libavutil': '>= 55.28.100',
  'libswscale': '>= 4.6.100',
}
ffmpeg_deps = []
ffmpeg_found = true
foreach name, version : ffmpeg_versions
  dep = dependency(name, version: version, required: ffmpeg_opt)
  ffmpeg_deps += dep
  if not dep.found()
    ffmpeg_found = false
    break
  endif
endforeach

if ffmpeg_found
  media_backends += 'ffmpeg'
  cdata.set('HAVE_FFMPEG', 1)
  shared_module('media-ffmpeg',
    sources: 'gtkffmediafile.c',
    c_args: extra_c_args,
    dependencies: [ libgtk_dep, ffmpeg_deps ],
    name_suffix: module_suffix,
    install_dir: media_install_dir,
    install: true,
  )
endif

gstplayer_dep = dependency('gstreamer-player-1.0', version: '>= 1.12.3',
                           required: get_option('media-gstreamer'))
gstgl_dep = dependency('gstreamer-gl-1.0', version: '>= 1.12.3',
                       required: get_option('media-gstreamer'))

if gstplayer_dep.found() and gstgl_dep.found()
  extra_win_cflags = []

  if host_machine.system() == 'windows' and gstgl_dep.version().version_compare('>=1.19.1')
    message('libgstgl has gst_gl_display_new_with_type()')
    extra_win_cflags += '-DHAVE_GST_GL_DISPLAY_NEW_WITH_TYPE'
  endif

  media_backends += 'gstreamer'
  cdata.set('HAVE_GSTREAMER', 1)
  shared_module('media-gstreamer',
    sources: [
      'gtkgstmediafile.c',
      'gtkgstpaintable.c',
      'gtkgstsink.c',
    ],
    c_args: extra_c_args + extra_win_cflags,
    dependencies: [ libm, libgtk_dep, gstplayer_dep, gstgl_dep ],
    name_suffix: module_suffix,
    install_dir: media_install_dir,
    install: true,
  )
endif

if media_backends.length() > 0
  gio_module_dirs += media_install_dir
endif
