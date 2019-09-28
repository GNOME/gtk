subdir('parser')
subdir('nodes')
subdir('style')

testexecdir = join_paths(installed_test_bindir, 'css')
testdatadir = join_paths(installed_test_datadir, 'css')

test_api = executable('api', 'api.c',
                      dependencies: libgtk_dep,
                      install: get_option('installed_tests'),
                      install_dir: testexecdir)
test('css/api', test_api)

if get_option('installed_tests')
  conf = configuration_data()
  conf.set('libexecdir', gtk_libexecdir)
  configure_file(input: 'api.test.in',
                 output: 'api.test',
                 configuration: conf,
                 install_dir: testdatadir)
endif
