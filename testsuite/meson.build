gtk_libexecdir = join_paths(gtk_prefix, get_option('libexecdir'))
installed_test_bindir = join_paths(gtk_libexecdir, 'installed-tests', 'gtk-4.0')
installed_test_datadir = join_paths(gtk_datadir, 'installed-tests', 'gtk-4.0')

common_env = [
  'GIO_USE_VOLUME_MONITOR=unix',
  'GSETTINGS_BACKEND=memory',
  'GTK_CSD=1',
  'G_ENABLE_DIAGNOSTIC=0',
  'GSETTINGS_SCHEMA_DIR=@0@'.format(gtk_schema_build_dir),
  'GDK_DEBUG=default-settings',
]

if x11_enabled
  add_test_setup ('x11',
                  env: common_env + [
                       'GDK_BACKEND=x11',
                       'TEST_OUTPUT_SUBDIR=x11',
                       ])
endif

if wayland_enabled
  add_test_setup ('wayland',
                  is_default: true,
                  env: common_env + [
                        'GDK_BACKEND=wayland',
                        'TEST_OUTPUT_SUBDIR=wayland',
                        ])

  add_test_setup ('waylandgles',
                  env: common_env + [
                        'GDK_BACKEND=wayland',
                        'TEST_OUTPUT_SUBDIR=waylandgles',
                        'GDK_DEBUG=gl-gles,default-settings',
                        ])

endif

if os_win32
  add_test_setup ('win32',
                  env: common_env + [
                        'GDK_BACKEND=win32',
                        'TEST_OUTPUT_SUBDIR=win32',
                        ])
endif

if broadway_enabled
  add_test_setup ('broadway',
                  env: common_env + [
                        'GDK_BACKEND=broadway',
                        'TEST_OUTPUT_SUBDIR=broadway',
                        ])
endif

subdir('performance')
subdir('gdk')
subdir('gsk')
subdir('gtk')
subdir('ottie')
subdir('css')
subdir('a11y')
subdir('tools')
subdir('reftests')

if build_gir
  subdir('introspection')
endif
