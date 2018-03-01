all_media_backends = [
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

