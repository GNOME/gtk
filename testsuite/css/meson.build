subdir('parser')
subdir('nodes')
subdir('style')
subdir('change')

testexecdir = join_paths(installed_test_bindir, 'css')
testdatadir = join_paths(installed_test_datadir, 'css')

test_api = executable('api', 'api.c',
                      dependencies: libgtk_dep,
                      install: get_option('install-tests'),
                      install_dir: testexecdir)
test('api', test_api,
     args: ['--tap', '-k' ],
     env: [ 'GIO_USE_VOLUME_MONITOR=unix',
            'GSETTINGS_BACKEND=memory',
            'GTK_CSD=1',
            'G_ENABLE_DIAGNOSTIC=0',
            'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
            'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir())
          ],
     suite: 'css')

test_data = executable('data', ['data.c', '../../gtk/css/gtkcssdataurl.c'],
                       include_directories: [confinc, ],
                       dependencies: gtk_deps,
                       install: get_option('install-tests'),
                       install_dir: testexecdir)
test('data', test_data,
     args: ['--tap', '-k' ],
     env: [ 'GIO_USE_VOLUME_MONITOR=unix',
            'GSETTINGS_BACKEND=memory',
            'GTK_CSD=1',
            'G_ENABLE_DIAGNOSTIC=0',
            'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
            'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir())
          ],
     suite: 'css')

if get_option('install-tests')
  conf = configuration_data()
  conf.set('libexecdir', gtk_libexecdir)
  configure_file(input: 'api.test.in',
                 output: 'api.test',
                 configuration: conf,
                 install_dir: testdatadir)
endif
