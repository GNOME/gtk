testexecdir = join_paths(installed_test_bindir, 'gtk')
testdatadir = join_paths(installed_test_datadir, 'gtk')

gtk_tests_export_dynamic_ldflag = []

if cc.get_id() != 'msvc'
  if os_darwin
    gtk_tests_export_dynamic_ldflag = ['-Wl,-export_dynamic']
  else
    gtk_tests_export_dynamic_ldflag = ['-Wl,--export-dynamic']
  endif
endif

# Available keys for each test:
#
#  - 'name': the test name; used for the test and to determine the base
#            source file for the test (MANDATORY)
#  - 'sources': (array): additional sources needed by the test
#  - 'c_args': (array): additional compiler arguments
#  - 'link_args': (array): additional linker arguments
#  - 'suites': (array): additional test suites
tests = [
  { 'name': 'accel' },
# sadly, mesons xfail support seems busted
#  { 'name': 'accessor-apis' },
  { 'name': 'action' },
  { 'name': 'adjustment' },
  { 'name': 'bitset' },
  {
    'name': 'bitmask',
    'sources': ['../../gtk/gtkallocatedbitmask.c'],
    'c_args': ['-DGTK_COMPILATION', '-UG_ENABLE_DEBUG'],
  },
  {
    'name': 'builder',
    'link_args': gtk_tests_export_dynamic_ldflag,
  },
  { 'name': 'builderparser' },
  { 'name': 'cellarea' },
  { 'name': 'check-icon-names' },
  {
    'name': 'constraint-solver',
    'sources': [
      '../../gtk/gtkconstraintsolver.c',
      '../../gtk/gtkconstraintexpression.c',
    ],
    'c_args': ['-DGTK_COMPILATION', '-UG_ENABLE_DEBUG'],
  },
  { 'name': 'cssprovider' },
  {
    'name': 'rbtree-crash',
    'sources': ['../../gtk/gtkrbtree.c'],
    'c_args': ['-DGTK_COMPILATION', '-UG_ENABLE_DEBUG'],
  },
  { 'name': 'defaultvalue' },
  { 'name': 'entry' },
  { 'name': 'expression' },
  { 'name': 'filter' },
  { 'name': 'filterlistmodel' },
  {
    'name': 'filterlistmodel-exhaustive',
    'suites': ['slow'],
  },
  { 'name': 'flattenlistmodel' },
  { 'name': 'floating' },
  { 'name': 'flowbox' },
  #{ 'name': 'gestures' },
  { 'name': 'grid' },
  { 'name': 'grid-layout' },
  { 'name': 'icontheme' },
  { 'name': 'listbox' },
  { 'name': 'main' },
  { 'name': 'maplistmodel' },
  { 'name': 'multiselection' },
  { 'name': 'notify' },
  { 'name': 'no-gtk-init' },
  { 'name': 'object' },
  { 'name': 'objects-finalize' },
  { 'name': 'papersize' },
  #{ 'name': 'popover' },
  {
    'name': 'propertylookuplistmodel',
    'sources': ['../../gtk/gtkpropertylookuplistmodel.c'],
    'c_args': ['-DGTK_COMPILATION', '-UG_ENABLE_DEBUG'],
  },
  {
    'name': 'rbtree',
    'sources': ['../../gtk/gtktreerbtree.c'],
    'c_args': ['-DGTK_COMPILATION', '-UG_ENABLE_DEBUG'],
  },
  { 'name': 'recentmanager' },
  { 'name': 'regression-tests' },
  { 'name': 'scrolledwindow' },
  { 'name': 'searchbar' },
  { 'name': 'shortcuts' },
  { 'name': 'singleselection' },
  { 'name': 'slicelistmodel' },
  { 'name': 'sorter' },
  { 'name': 'sortlistmodel' },
  { 'name': 'sortlistmodel-exhaustive' },
  { 'name': 'spinbutton' },
  { 'name': 'stringlist' },
  { 'name': 'templates' },
  { 'name': 'textbuffer' },
  { 'name': 'textiter' },
  { 'name': 'theme-validate' },
  {
    'name': 'timsort',
    'sources': ['timsort.c', '../../gtk/timsort/gtktimsort.c'],
  },
  { 'name': 'tooltips' },
  { 'name': 'treelistmodel' },
  {
    'name': 'treemodel',
    'sources': [
      'treemodel.c',
      'liststore.c',
      'treestore.c',
      'filtermodel.c',
      'modelrefcount.c',
      'sortmodel.c',
      'gtktreemodelrefcount.c',
    ],
  },
  { 'name': 'treepath' },
  { 'name': 'treesorter' },
  { 'name': 'treeview' },
  { 'name': 'typename' },
  { 'name': 'displayclose' },
  { 'name': 'revealer-size' },
  { 'name': 'widgetorder' },
  { 'name': 'widget-refcount' },
]

# Tests that are expected to fail
xfail = [
  # we are still missing some accessors
  'accessor-apis',
  # one of the window resizing tests fails after
  # the GdkToplevel refactoring, and needs a big
  # gtkwindow.c configure request cleanup
  'window',
]

is_debug = get_option('buildtype').startswith('debug')

test_cargs = []

test_env = environment()
test_env.set('GTK_TEST_ACCESSIBLE', '1')
test_env.set('GSK_RENDERER', 'cairo')
test_env.set('G_TEST_SRCDIR', meson.current_source_dir())
test_env.set('G_TEST_BUILDDIR', meson.current_build_dir())
test_env.set('GIO_USE_VFS', 'local')
test_env.set('GSETTINGS_BACKEND', 'memory')
test_env.set('G_ENABLE_DIAGNOSTIC', '0')

if os_unix
  # tests += [['defaultvalue']]  # disabled in Makefile.am as well
  test_cargs += ['-DHAVE_UNIX_PRINT_WIDGETS']
endif

foreach flag: common_cflags
  if flag not in ['-Werror=missing-prototypes', '-Werror=missing-declarations', '-fvisibility=hidden']
    test_cargs += flag
  endif
endforeach

foreach t : tests
  test_name = t.get('name')
  test_srcs = ['@0@.c'.format(test_name)] + t.get('sources', [])
  test_extra_cargs = t.get('c_args', [])
  test_extra_ldflags = t.get('link_args', [])
  test_extra_suites = t.get('suites', [])
  test_timeout = 60

  test_exe = executable(test_name, test_srcs,
    c_args : test_cargs + test_extra_cargs,
    link_args : test_extra_ldflags,
    dependencies : libgtk_dep,
    install: get_option('install-tests'),
    install_dir: testexecdir)

  expect_fail = xfail.contains(test_name)

  if test_extra_suites.contains('slow')
    test_timeout = 90
  endif

  test(test_name, test_exe,
       args: [ '--tap', '-k' ],
       protocol: 'tap',
       timeout: test_timeout,
       env: test_env,
       suite: ['gtk'] + test_extra_suites,
       should_fail: expect_fail,
  )
endforeach

# FIXME: if objc autotestkeywords_CPPFLAGS += -DHAVE_OBJC=1 -x objective-c++
if add_languages('cpp', required: false)
  test_exe = executable('autotestkeywords',
                        'autotestkeywords.cc',
                        c_args : test_cargs + ['-Idummy-headers'],
                        dependencies : libgtk_dep,
                        install: get_option('install-tests'),
                        install_dir: testexecdir)
  test('c++ keywords', test_exe,
       args: [ '--tap', '-k' ],
       #protocol: 'tap',
       env: test_env,
       suite: 'gtk')
  if get_option('install-tests')
    conf = configuration_data()
    conf.set('testexecdir', testexecdir)
    conf.set('test', 'autotestkeywords')
    configure_file(input: 'gtk.test.in',
                   output: 'autotestkeywords.test',
                   configuration: conf,
                   install_dir: testdatadir)
  endif
endif


focus_chain_tests = [
  # test     direction
  [ 'basic', 'tab' ],
  [ 'basic', 'tab-backward' ],
  [ 'basic', 'left' ],
  [ 'basic', 'right' ],
  [ 'widget-factory', 'tab' ],
  [ 'widget-factory', 'tab-backward' ],
  [ 'widget-factory', 'up' ],
  [ 'widget-factory', 'down' ],
  [ 'widget-factory', 'left' ],
  # this one is flaky in ci, for unclear reasons
  #[ 'widget-factory', 'right' ],
  [ 'widget-factory2', 'tab' ],
  [ 'widget-factory2', 'tab-backward' ],
  # in ci, we don't show a color picker, so the focus chain
  # for page3 is missing an expected button in the color editor
  #[ 'widget-factory3', 'tab' ],
  #[ 'widget-factory3', 'tab-backward' ],
]

focus_chain = executable(
  'test-focus-chain',
  ['test-focus-chain.c'],
  dependencies: libgtk_dep,
  c_args: common_cflags,
  install: get_option('install-tests'),
  install_dir: testexecdir
)

foreach test : focus_chain_tests
  test(test[0] + ' ' + test[1], focus_chain,
       args: [ join_paths(meson.current_source_dir(), 'focus-chain', test[0] + '.ui'),
               join_paths(meson.current_source_dir(), 'focus-chain', test[0] + '.' + test[1]) ],
       env: test_env,
       suite: [ 'gtk', 'focus' ])
endforeach


if get_option('install-tests')
  foreach t : tests
    test_name = t.get('name')
    conf = configuration_data()
    conf.set('testexecdir', testexecdir)
    conf.set('test', test_name)
    configure_file(input: 'gtk.test.in',
                   output: '@0@.test'.format(test_name),
                   configuration: conf,
                   install_dir: testdatadir)
  endforeach

  install_subdir('icons', install_dir: testexecdir)
  install_subdir('icons2', install_dir: testexecdir)
  install_subdir('ui', install_dir: testexecdir)

endif

if false and get_option ('profiler')

  performance_env = test_env
  performance_env.set('GTK_THEME', 'Empty')

  test('performance-layout', test_performance,
       args: [ '--mark', 'size allocation', join_paths(meson.current_build_dir(), '../../demos/widget-factory/gtk4-widget-factory') ],
       env: performance_env,
       suite: [ 'gtk' ])

  test('performance-snapshot', test_performance,
       args: [ '--mark', 'widget snapshot', join_paths(meson.current_build_dir(), '../../demos/widget-factory/gtk4-widget-factory') ],
       env: performance_end,
       suite: [ 'gtk' ])

endif
