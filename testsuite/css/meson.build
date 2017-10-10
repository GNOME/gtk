subdir('parser')
subdir('nodes')
subdir('style')

test_api = executable('api', 'api.c',
                      install: get_option('install-tests'),
                      install_dir: join_paths(installed_test_bindir, 'css'),
                      dependencies: libgtk_dep)
test('css/api', test_api)

if get_option('install-tests')
  conf = configuration_data()
  conf.set('libexecdir', gtk_libexecdir)
  configure_file(input: 'api.test.in',
                 output: 'api.test',
                 configuration: conf,
                 install_dir: installed_test_datadir)
endif

