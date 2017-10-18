subdir('parser')
subdir('nodes')
subdir('style')

testexecdir = join_paths(installed_test_bindir, 'css')
testdatadir = join_paths(installed_test_datadir, 'css')

test_api = executable('api', 'api.c',
                      dependencies: libgtk_dep,
                      install: get_option('install-tests'),
                      install_dir: testexecdir)
test('api', test_api,
     args: ['--tap', '-k' ],
     env: installed_test_env,
     suite: 'css')

if get_option('install-tests')
  conf = configuration_data()
  conf.set('libexecdir', gtk_libexecdir)
  configure_file(input: 'api.test.in',
                 output: 'api.test',
                 configuration: conf,
                 install_dir: testdatadir)
endif
