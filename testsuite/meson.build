gtk_libexecdir = join_paths(gtk_prefix, get_option('libexecdir'))
installed_test_bindir = join_paths(gtk_libexecdir, 'installed-tests', 'gtk+')
installed_test_datadir = join_paths(gtk_datadir, 'installed-tests', 'gtk+')

if meson.version().version_compare('>=0.57.0')
  add_test_setup('default',
    is_default: true,
    exclude_suites: ['flaky', 'failing'],
  )
endif

subdir('gtk')
subdir('gdk')
subdir('css')
subdir('a11y')
subdir('tools')
subdir('reftests')
