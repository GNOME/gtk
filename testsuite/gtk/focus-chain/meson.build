testexecdir = join_paths(installed_test_bindir, 'gtk', 'focus-chain')
testdatadir = join_paths(installed_test_datadir, 'gtk', 'focus-chain')

test_nodes = executable('test-focus', 'test-focus.c',
                        install: get_option('install-tests'),
                        install_dir: testexecdir,
                        dependencies: libgtk_dep)
test('nodes', test_nodes,
     args: [ '--tap', '-k' ],
     env: [ 'GIO_USE_VOLUME_MONITOR=unix',
            'GSETTINGS_BACKEND=memory',
            'GTK_CSD=1',
            'G_ENABLE_DIAGNOSTIC=0',
            'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
            'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir())
          ],
     suite: 'css')

test_data = [
  'widget-factory.ui',
  'widget-factory.txt',
]

if get_option('install-tests')
  conf = configuration_data()
  conf.set('libexecdir', gtk_libexecdir)
  configure_file(input: 'focus.test.in',
                 output: 'focus.test',
                 configuration: conf,
                 install_dir: testdatadir)

  install_data(test_data, install_dir: testexecdir)

endif
