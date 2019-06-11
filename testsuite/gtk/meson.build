testexecdir = join_paths(installed_test_bindir, 'gtk')
testdatadir = join_paths(installed_test_datadir, 'gtk')

gtk_tests_export_dynamic_ldflag = []

if cc.get_id() != 'msvc'
  gtk_tests_export_dynamic_ldflag = ['-Wl,--export-dynamic']
endif

tests = [
  ['popover'],
  ['accel'],
  ['accessible'],
  ['adjustment'],
  ['bitmask', ['../../gtk/gtkallocatedbitmask.c'], ['-DGTK_COMPILATION', '-UG_ENABLE_DEBUG']],
  ['builder', [], [], gtk_tests_export_dynamic_ldflag],
  ['builderparser'],
  ['cellarea'],
  ['check-icon-names'],
  ['cssprovider'],
  ['rbtree-crash', ['../../gtk/gtkrbtree.c'], ['-DGTK_COMPILATION', '-UG_ENABLE_DEBUG']],
  ['defaultvalue'],
  ['entry'],
  ['filterlistmodel'],
  ['flattenlistmodel'],
  ['firefox-stylecontext'],
  ['floating'],
  ['focus'],
  ['gestures'],
  ['grid'],
  ['gtkmenu'],
  ['icontheme'],
  ['keyhash', ['../../gtk/gtkkeyhash.c', gtkresources, '../../gtk/gtkprivate.c'], gtk_cargs],
  ['listbox'],
  ['main'],
  ['maplistmodel'],
  ['notify'],
  ['no-gtk-init'],
  ['object'],
  ['objects-finalize'],
  ['papersize'],
  ['propertylookuplistmodel', ['../../gtk/gtkpropertylookuplistmodel.c'], ['-DGTK_COMPILATION', '-UG_ENABLE_DEBUG']],
  ['rbtree', ['../../gtk/gtktreerbtree.c'], ['-DGTK_COMPILATION', '-UG_ENABLE_DEBUG']],
  ['recentmanager'],
  ['regression-tests'],
  ['scrolledwindow'],
  ['searchbar'],
  ['singleselection'],
  ['slicelistmodel'],
  ['sortlistmodel'],
  ['spinbutton'],
  ['stylecontext'],
  ['templates'],
  ['textbuffer'],
  ['textiter'],
  ['theme-validate'],
  ['treelistmodel'],
  ['treemodel', ['treemodel.c', 'liststore.c', 'treestore.c', 'filtermodel.c',
                 'modelrefcount.c', 'sortmodel.c', 'gtktreemodelrefcount.c']],
  ['treepath'],
  ['treeview'],
  ['typename'],
  ['window'],
  ['displayclose'],
  ['revealer-size'],
  ['widgetorder'],
]

test_cargs = []

if os_unix
  # tests += [['defaultvalue']]  # disabled in Makefile.am as well
  test_cargs += ['-DHAVE_UNIX_PRINT_WIDGETS']
endif

foreach t : tests
  test_name = t.get(0)
  test_srcs = ['@0@.c'.format(test_name)] + t.get(1, [])
  test_extra_cargs = t.get(2, [])
  test_extra_ldflags = t.get(3, [])

  test_exe = executable(test_name, test_srcs,
    c_args : test_cargs + test_extra_cargs,
    link_args : test_extra_ldflags,
    dependencies : libgtk_dep,
    install: get_option('install-tests'),
    install_dir: testexecdir)

  test(test_name, test_exe,
       args: [ '--tap', '-k' ],
       env: [ 'GIO_USE_VOLUME_MONITOR=unix',
              'GSETTINGS_BACKEND=memory',
              'GTK_CSD=1',
              'G_ENABLE_DIAGNOSTIC=0',
              'GSK_RENDERER=cairo',
              'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
              'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
              'GSETTINGS_SCHEMA_DIR=@0@'.format(gtk_schema_build_dir),
            ],
       suite: 'gtk')
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
       env: [ 'GIO_USE_VOLUME_MONITOR=unix',
              'GSETTINGS_BACKEND=memory',
              'GTK_CSD=1',
              'G_ENABLE_DIAGNOSTIC=0',
              'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
              'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
              'GSETTINGS_SCHEMA_DIR=@0@'.format(gtk_schema_build_dir),
            ],
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

focus_chain = executable(
  'test-focus-chain',
  ['test-focus-chain.c'],
  dependencies: libgtk_dep,
  install: get_option('install-tests'),
  install_dir: testexecdir
)

foreach test : focus_chain_tests
  test(test[0] + ' ' + test[1], focus_chain,
       args: [ join_paths(meson.current_source_dir(), 'focus-chain', test[0] + '.ui'),
               join_paths(meson.current_source_dir(), 'focus-chain', test[0] + '.' + test[1]) ],
       env: [ 'GIO_USE_VOLUME_MONITOR=unix',
              'GSETTINGS_BACKEND=memory',
              'GTK_CSD=1',
              'G_ENABLE_DIAGNOSTIC=0',
              'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
              'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir())
             ],
       suite: [ 'gtk', 'focus' ])
endforeach


if get_option('install-tests')
  foreach t : tests
    test_name = t.get(0)
    conf = configuration_data()
    conf.set('testexecdir', testexecdir)
    conf.set('test', test_name)
    configure_file(input: 'gtk.test.in',
                   output: '@0@.test'.format(test_name),
                   configuration: conf,
                   install_dir: testdatadir)
  endforeach

  install_subdir('icons', install_dir: testexecdir)
  install_subdir('ui', install_dir: testexecdir)

endif

