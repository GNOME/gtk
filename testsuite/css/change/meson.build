changetest_env = environment()
changetest_env.set('GTK_A11Y', 'test')
changetest_env.set('GSK_RENDERER', 'cairo')
changetest_env.set('G_TEST_SRCDIR', meson.current_source_dir())
changetest_env.set('G_TEST_BUILDDIR', meson.current_build_dir())
changetest_env.set('GIO_USE_VFS', 'local')
changetest_env.set('GSETTINGS_BACKEND', 'memory')
changetest_env.set('G_ENABLE_DIAGNOSTIC', '0')

testexecdir = join_paths(installed_test_bindir, 'css', 'change')
testdatadir = join_paths(installed_test_datadir, 'css')

test_change = executable(
  'test-css-change',
  'test-css-change.c',
  '../../testutils.c',
  c_args: common_cflags,
  dependencies: libgtk_dep,
  install: get_option('install-tests'),
  install_dir: testexecdir,
)

test('change', test_change,
  args: [ '--tap', '-k' ],
  protocol: 'tap',
  env: changetest_env,
  suite: 'css',
)

test_data = [
  'test1.css', 'test1.ui', 'test1.nodes',
  'test2.css', 'test2.ui', 'test2.nodes',
  'test3.css', 'test3.ui', 'test3.nodes',
  'test4.css', 'test4.ui', 'test4.nodes',
]

if get_option('install-tests')
  conf = configuration_data()
  conf.set('libexecdir', gtk_libexecdir)
  configure_file(input: 'change.test.in',
                 output: 'change.test',
                 configuration: conf,
                 install_dir: testdatadir)

  install_data(test_data, install_dir: testexecdir)

endif
