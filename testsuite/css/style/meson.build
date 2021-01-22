styletest_env = environment()
styletest_env.set('GTK_A11Y', 'test')
styletest_env.set('GSK_RENDERER', 'cairo')
styletest_env.set('G_TEST_SRCDIR', meson.current_source_dir())
styletest_env.set('G_TEST_BUILDDIR', meson.current_build_dir())
styletest_env.set('GIO_USE_VFS', 'local')
styletest_env.set('GSETTINGS_BACKEND', 'memory')
styletest_env.set('G_ENABLE_DIAGNOSTIC', '0')

testexecdir = join_paths(installed_test_bindir, 'css', 'style')
testdatadir = join_paths(installed_test_datadir, 'css')

cssresources = gnome.compile_resources(
  'cssresources',
  'test-css-style.gresource.xml',
  source_dir: '.',
)

test_style = executable(
  'test-css-style',
  'test-css-style.c',
  '../../testutils.c',
  cssresources,
  c_args: common_cflags,
  dependencies: libgtk_dep,
  install: get_option('install-tests'),
  install_dir: testexecdir,
)
test('style', test_style,
  args: [ '--tap', '-k' ],
  protocol: 'tap',
  env: styletest_env,
  suite: 'css',
)

test_data = [
  'adjacent-states.css',
  'adjacent-states.nodes',
  'adjacent-states.ui',
  'bloomfilter-not.css',
  'bloomfilter-not.nodes',
  'bloomfilter-not.ui',
  'colornames.css',
  'colornames.nodes',
  'colornames.ui',
  'currentcolor.css',
  'currentcolor.nodes',
  'currentcolor.ui',
  'font.css',
  'font.nodes',
  'font.ui',
  'gradient.css',
  'gradient.nodes',
  'gradient.ui',
  'gtk.css',
  'inherit.css',
  'inherit.nodes',
  'inherit.ui',
  'label.css',
  'label.nodes',
  'label.ui',
  'nth-child.css',
  'nth-child.nodes',
  'nth-child.ui',
]

if get_option('install-tests')
  conf = configuration_data()
  conf.set('libexecdir', gtk_libexecdir)
  configure_file(input: 'style.test.in',
                 output: 'style.test',
                 configuration: conf,
                 install_dir: testdatadir)

  install_data(test_data, install_dir: testexecdir)
endif
