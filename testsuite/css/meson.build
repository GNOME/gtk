subdir('parser')
subdir('nodes')
subdir('style')

gtk_libexecdir = join_paths(gtk_prefix, get_option('libexecdir'))
testdatadir = join_paths(gtk_datadir, 'installed-tests/gtk4')
testexecdir = join_paths(gtk_libexecdir, 'installed-tests/gtk4/css')

test_api = executable('api', 'api.c',
                      install: get_option('install-tests'),
                      install_dir: testexecdir,
                      dependencies: libgtk_dep)
test('css/api', test_api)

if get_option('install-tests')
  conf = configuration_data()
  conf.set('libexecdir', gtk_libexecdir)
  configure_file(input: 'api.test.in',
                 output: 'api.test',
                 configuration: conf,
                 install_dir: testdatadir)
endif

