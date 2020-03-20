subdir('parser')
subdir('nodes')
subdir('style')
subdir('change')

testexecdir = join_paths(installed_test_bindir, 'css')
testdatadir = join_paths(installed_test_datadir, 'css')

test_api = executable('api', 'api.c',
                      c_args: common_cflags,
                      dependencies: libgtk_dep,
                      install: get_option('install-tests'),
                      install_dir: testexecdir)
test('api', test_api,
     args: ['--tap', '-k' ],
     protocol: 'tap',
     env: [ 'GIO_USE_VOLUME_MONITOR=unix',
            'GSETTINGS_BACKEND=memory',
            'GDK_DEBUG=default-settings',
            'GTK_CSD=1',
            'G_ENABLE_DIAGNOSTIC=0',
            'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
            'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir())
          ],
     suite: 'css')

test_data = executable('data', ['data.c', '../../gtk/css/gtkcssdataurl.c'],
                       c_args: common_cflags,
                       include_directories: [confinc, ],
                       dependencies: gtk_deps,
                       install: get_option('install-tests'),
                       install_dir: testexecdir)
test('data', test_data,
     args: ['--tap', '-k' ],
     protocol: 'tap',
     env: [ 'GIO_USE_VOLUME_MONITOR=unix',
            'GSETTINGS_BACKEND=memory',
            'GDK_DEBUG=default-settings',
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

if get_option ('profiler')

  test('performance-adwaita', test_performance,
       args: [ '--mark', 'css validation',
               '--name',  'performance-adwaita',
               '--output', join_paths(meson.current_build_dir(), 'output'),
               join_paths(meson.current_build_dir(), '../../demos/widget-factory/gtk4-widget-factory') ],
       env: [ 'GTK_THEME=Adwaita',
              'GSETTINGS_SCHEMA_DIR=@0@'.format(gtk_schema_build_dir) ],
       suite: [ 'css' ])

  test('performance-empty', test_performance,
       args: [ '--mark', 'css validation',
               '--name',  'performance-empty',
               '--output', join_paths(meson.current_build_dir(), 'output'),
               join_paths(meson.current_build_dir(), '../../demos/widget-factory/gtk4-widget-factory') ],
       env: [ 'GTK_THEME=Empty',
              'GSETTINGS_SCHEMA_DIR=@0@'.format(gtk_schema_build_dir) ],
       suite: [ 'css' ])

endif
