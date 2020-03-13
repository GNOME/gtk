testexecdir = join_paths(installed_test_bindir, 'gdk')
testdatadir = join_paths(installed_test_datadir, 'gdk')

tests = [
  'cairo',
  'display',
  'encoding',
  'keysyms',
  'memorytexture',
  'rectangle',
  'rgba',
  'seat',
]

foreach t : tests
  test_exe = executable(t, '@0@.c'.format(t),
                        c_args: common_cflags,
                        dependencies: libgtk_dep,
                        install: get_option('install-tests'),
                        install_dir: testexecdir)

  test(t, test_exe,
       args: [ '--tap', '-k' ],
       env: [ 'GIO_USE_VOLUME_MONITOR=unix',
              'GSETTINGS_BACKEND=memory',
              'GDK_DEBUG=default-settings',
              'GTK_CSD=1',
              'G_ENABLE_DIAGNOSTIC=0',
              'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
              'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir())
            ],
       suite: 'gdk')

  if get_option('install-tests')
    test_cdata = configuration_data()
    test_cdata.set('testexecdir', testexecdir)
    test_cdata.set('test', t)
    configure_file(input: 'gdk.test.in',
                   output: '@0@.test'.format(t),
                   configuration: test_cdata,
                   install: true,
                   install_dir: testdatadir)
  endif
endforeach
