gtk_libexecdir = join_paths(gtk_prefix, get_option('libexecdir'))
installed_test_bindir = join_paths(gtk_libexecdir, 'installed-tests', 'gtk-4.0')
installed_test_datadir = join_paths(gtk_datadir, 'installed-tests', 'gtk-4.0')

# We call diff in various tests, so we need to check it's available,
# otherwise we're going to have failures down the line
diff = find_program('diff', required: true)

common_env = [
  'GIO_USE_VOLUME_MONITOR=unix',
  'GSETTINGS_BACKEND=memory',
  'GDK_DEBUG=default-settings',
  'GTK_CSD=1',
  'G_ENABLE_DIAGNOSTIC=0',
  'GSETTINGS_SCHEMA_DIR=@0@'.format(gtk_schema_build_dir),
]

add_test_setup ('x11',
                is_default: true,
                env: common_env + [
                     'GDK_BACKEND=x11',
                     'TEST_OUTPUT_SUBDIR=x11',
                     ])

add_test_setup ('wayland',
                env: common_env + [
                      'GDK_BACKEND=wayland',
                      'TEST_OUTPUT_SUBDIR=wayland',
                      ])

add_test_setup ('broadway',
                env: common_env + [
                      'GDK_BACKEND=broadway',
                      'TEST_OUTPUT_SUBDIR=broadway',
                      ])

subdir('performance')
subdir('gdk')
subdir('gsk')
subdir('gtk')
subdir('css')
subdir('a11y')
subdir('tools')
subdir('reftests')
