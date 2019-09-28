tests = [
  'cairo',
  'display',
  'encoding',
  'keysyms',
  'rectangle',
  'rgba',
  'seat',
  'visual',
]

foreach t : tests
  test_exe = executable(t, '@0@.c'.format(t),
                        dependencies: libgtk_dep,
                        install: get_option('installed_tests'),
                        install_dir: installed_test_bindir)

  test(t, test_exe,
       args: [ '--tap', '-k' ],
       env: [ 'GIO_USE_VOLUME_MONITOR=unix',
              'GSETTINGS_BACKEND=memory',
              'GTK_CSD=1',
              'G_ENABLE_DIAGNOSTIC=0',
              'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
              'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir())
            ],
       suite: 'gdk')
endforeach
