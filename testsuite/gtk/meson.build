testexecdir = join_paths(installed_test_bindir, 'gtk')
testdatadir = join_paths(installed_test_datadir, 'gtk')

gtk_tests_export_dynamic_ldflag = []

if cc.get_id() != 'msvc'
  gtk_tests_export_dynamic_ldflag = ['-Wl,--export-dynamic']
endif

is_debug = get_option('buildtype').startswith('debug')

# An array of dictionaries with the following keys:
#
#  - name: the name of the test; also used to generate the
#          name of the source file with the test (MANDATORY)
#  - sources: an array of additional source files (OPTIONAL)
#  - cflags: an array of additonal compiler flags (OPTIONAL)
#  - ldflags: an array of additional linker flags (OPTIONAL)
#  - suites: an array of additional suite names (OPTIONAL)
#  - xfail: a boolean value indicating whether the test is
#           expected to fail (OPTIONAL)
#
tests = [
  { 'name': 'accel' },
  { 'name': 'accessible' },
  { 'name': 'action' },
  { 'name': 'adjustment' },
  { 'name': 'bitmask',
    'sources': ['../../gtk/gtkallocatedbitmask.c'],
    'cflags': ['-DGTK_COMPILATION', '-UG_ENABLE_DEBUG'],
    'suites': ['internals'],
  },
  { 'name': 'builder',
    'ldflags': gtk_tests_export_dynamic_ldflag,
  },
  { 'name': 'builderparser' },
  { 'name': 'cellarea' },
  { 'name': 'check-icon-names' },
  { 'name': 'constraint-solver',
    'sources': [
      '../../gtk/gtkconstraintsolver.c',
      '../../gtk/gtkconstraintexpression.c',
    ],
    'cflags': ['-DGTK_COMPILATION', '-UG_ENABLE_DEBUG'],
    'suites': ['internals'],
  },
  { 'name': 'cssprovider' },
  { 'name': 'rbtree-crash',
    'sources': ['../../gtk/gtkrbtree.c'],
    'cflags': ['-DGTK_COMPILATION', '-UG_ENABLE_DEBUG'],
    'suites': ['internals'],
  },
  { 'name': 'defaultvalue' },
  { 'name': 'entry' },
  { 'name': 'filterlistmodel' },
  { 'name': 'flattenlistmodel' },
  { 'name': 'floating' },
  { 'name': 'focus' },
  { 'name': 'gestures' },
  { 'name': 'grid' },
  { 'name': 'grid-layout' },
  { 'name': 'icontheme' },
  { 'name': 'keyhash',
    'sources': [
      '../../gtk/gtkkeyhash.c',
      gtkresources,
      '../../gtk/gtkprivate.c',
    ],
    'cflags': gtk_cargs,
    'suites': ['internals'],
  },
  { 'name': 'listbox' },
  { 'name': 'main' },
  { 'name': 'maplistmodel' },
  { 'name': 'notify' },
  { 'name': 'no-gtk-init' },
  { 'name': 'object' },
  { 'name': 'objects-finalize' },
  { 'name': 'papersize' },
  { 'name': 'popover' },
  { 'name': 'propertylookuplistmodel',
    'sources': ['../../gtk/gtkpropertylookuplistmodel.c'],
    'cflags': ['-DGTK_COMPILATION', '-UG_ENABLE_DEBUG'],
    'suites': ['internals'],
  },
  { 'name': 'rbtree',
    'sources': ['../../gtk/gtktreerbtree.c'],
    'cflags': ['-DGTK_COMPILATION', '-UG_ENABLE_DEBUG'],
    'suites': ['internals'],
  },
  { 'name': 'recentmanager' },
  { 'name': 'regression-tests' },
  { 'name': 'scrolledwindow' },
  { 'name': 'searchbar' },
  { 'name': 'singleselection' },
  { 'name': 'slicelistmodel' },
  { 'name': 'sortlistmodel' },
  { 'name': 'spinbutton' },
  { 'name': 'templates' },
  { 'name': 'textbuffer' },
  { 'name': 'textiter' },
  { 'name': 'theme-validate' },
  { 'name': 'treelistmodel' },
  { 'name': 'treemodel',
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
  { 'name': 'treeview' },
  { 'name': 'typename' },
  { 'name': 'window' },
  { 'name': 'displayclose' },
  { 'name': 'revealer-size' },
  { 'name': 'widgetorder' },
]

test_cargs = []

if os_unix
  test_cargs += ['-DHAVE_UNIX_PRINT_WIDGETS']
endif

test_env = environment()
test_env.set('GIO_USE_VOLUME_MONITOR', 'unix')
test_env.set('GSETTINGS_BACKEND', 'memory')
test_env.set('GDK_DEBUG', 'default-settings')
test_env.set('GTK_CSD', '1')
test_env.set('G_ENABLE_DIAGNOSTIC', '0')
test_env.set('GSK_RENDERER', 'cairo')
test_env.set('G_TEST_SRCDIR', meson.current_source_dir())
test_env.set('G_TEST_BUILDDIR', meson.current_build_dir())
test_env.set('GSETTINGS_SCHEMA_DIR', gtk_schema_build_dir)

foreach t : tests
  test_name = t['name']

  test_srcs = ['@0@.c'.format(test_name)] + t.get('sources', [])
  test_extra_cargs = t.get('cflags', [])
  test_extra_ldflags = t.get('ldflags', [])
  test_extra_suites = t.get('suites', [])
  test_xfail = t.get('xfail', false)

  test_exe = executable(test_name,
    test_srcs,
    c_args: test_cargs + test_extra_cargs,
    link_args: test_extra_ldflags,
    dependencies: libgtk_dep,
    install: get_option('install-tests'),
    install_dir: testexecdir,
  )

  test(test_name, test_exe,
    args: [ '--tap', '-k' ],
    protocol: 'tap',
    env: test_env,
    suite: ['gtk'] + test_extra_suites,
    should_fail: test_xfail,
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
       protocol: 'tap',
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
  [ 'focusable-container', 'tab' ],
  [ 'focusable-container', 'tab-backward' ],
]

focus_chain = executable('test-focus-chain',
  ['test-focus-chain.c'],
  dependencies: libgtk_dep,
  install: get_option('install-tests'),
  install_dir: testexecdir,
)

foreach test : focus_chain_tests
  test(test[0] + ' ' + test[1],
    focus_chain,
    args: [
      meson.current_source_dir() / 'focus-chain' / test[0] + '.ui',
      meson.current_source_dir() / 'focus-chain' / test[0] + '.' + test[1],
    ],
    env: test_env,
    suite: [ 'gtk', 'focus' ],
  )
endforeach

if get_option('install-tests')
  foreach t: tests
    test_name = t.get(0)
    conf = configuration_data()
    conf.set('testexecdir', testexecdir)
    conf.set('test', test_name)
    configure_file(
      input: 'gtk.test.in',
      output: '@0@.test'.format(test_name),
      configuration: conf,
      install_dir: testdatadir,
    )
  endforeach

  install_subdir('icons', install_dir: testexecdir)
  install_subdir('ui', install_dir: testexecdir)
endif

if get_option ('profiler')
  test('performance-layout',
    test_performance,
    args: [
      '--mark',
      'size allocation',
      meson.current_build_dir() / '../../demos/widget-factory/gtk4-widget-factory',
    ],
    env: [
      'GTK_THEME=Empty',
      'GSETTINGS_SCHEMA_DIR=@0@'.format(gtk_schema_build_dir),
    ],
    suite: [ 'gtk' ],
  )

  test('performance-snapshot',
    test_performance,
    args: [
      '--mark', 'widget snapshot',
      meson.current_build_dir() / '../../demos/widget-factory/gtk4-widget-factory',
    ],
    env: [
      'GTK_THEME=Empty',
      'GSETTINGS_SCHEMA_DIR=@0@'.format(gtk_schema_build_dir),
    ],
    suite: [ 'gtk' ],
  )
endif
