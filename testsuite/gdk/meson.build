testexecdir = join_paths(installed_test_bindir, 'gdk')
testdatadir = join_paths(installed_test_datadir, 'gdk')

tests = [
  'cairo',
  'cursor',
  'display',
  'encoding',
  'keysyms',
  'rectangle',
  'rgba',
  'seat',
]

foreach t : tests
  test_exe = executable(t, '@0@.c'.format(t),
                        dependencies: libgtk_dep,
                        install: get_option('install-tests'),
                        install_dir: testexecdir)

  test(t, test_exe,
       args: [ '--tap', '-k' ],
       env: installed_test_env,
       suite: 'gdk')

endforeach

if get_option('install-tests')
  test_cdata = configuration_data()
  test_cdata.set('libexecdir', gtk_libexecdir)

  foreach t : tests
    configure_file(input: '@0@.test.in'.format(t),
                   output: '@0@.test'.format(t),
                   configuration: test_cdata,
                   install: true,
                   install_dir: testdatadir)
  endforeach

endif
