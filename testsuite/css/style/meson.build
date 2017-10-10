gtk_libexecdir = join_paths(gtk_prefix, get_option('libexecdir'))
testdatadir = join_paths(gtk_datadir, 'installed-tests/gtk4')
testexecdir = join_paths(gtk_libexecdir, 'installed-tests/gtk4/css/style')

cssresources = gnome.compile_resources(
  'cssresources',
  'test-css-style.gresource.xml',
  source_dir: '.',
)

test_style = executable(
  'test-css-style',
  'test-css-style.c',
  cssresources,
  install: get_option('install-tests'),
  install_dir: testexecdir,
  dependencies: libgtk_dep,
)
test('css/style/test-style', test_style)

test_data = [
  'adjacent-states.css',
  'adjacent-states.nodes',
  'adjacent-states.ui',
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
  configure_file(input: 'test-css-style.test.in',
                 output: 'test-css-style.test',
                 configuration: conf,
                 install_dir: testdatadir)

  install_data(test_data, install_dir: testexecdir)

endif

