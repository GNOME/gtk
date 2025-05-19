media_sources = []
media_deps = []
media_backends = []

if gstplay_dep.found() and gstgl_dep.found() and gstallocators_dep.found()
  media_backends += 'gstreamer'
  cdata.set('HAVE_MEDIA_GSTREAMER', 1)
  media_deps += [ libm, gstplay_dep, gstgl_dep, gstallocators_dep, gstd3d12_dep  ]
  media_sources += files([
    'gtkgstmediafile.c',
    'gtkgstpaintable.c',
    'gtkgstsink.c',
  ])
endif
