if get_option('installed_tests')
  # error('installed tests haven\'t been ported to meson yet')
endif

gtk_libexecdir = join_paths(gtk_prefix, get_option('libexecdir'))
installed_test_bindir = join_paths(gtk_libexecdir, 'installed-tests', 'gtk+')
installed_test_datadir = join_paths(gtk_datadir, 'installed-tests', 'gtk+')

subdir('gtk')
subdir('gdk')
subdir('css')
subdir('a11y')
subdir('tools')
subdir('reftests')