installed_test_bindir = join_paths(get_option('libexecdir'), 'installed-tests', 'gtk-4.0')
installed_test_datadir = join_paths(get_option('datadir'), 'installed-tests', 'gtk-4.0')

installed_test_env = [
  'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
  'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
  'GIO_USE_VOLUME_MONITOR=unix',
  'GSETTINGS_BACKEND=memory',
  'GTK_CSD=1',
  'G_ENABLE_DIAGNOSTIC=0',
]

subdir('gsk')
subdir('tools')
subdir('gtk')
subdir('gdk')
subdir('css')
subdir('a11y')
subdir('reftests')
