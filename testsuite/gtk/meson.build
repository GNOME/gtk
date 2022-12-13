gtk_tests_export_dynamic_ldflag = []

if cc.get_id() != 'msvc' and cc.get_id() != 'clang'
  gtk_tests_export_dynamic_ldflag = ['-Wl,--export-dynamic']
endif

tests = [
  ['accel'],
  ['accessible'],
  ['action'],
  ['adjustment'],
  ['bitmask', ['../../gtk/gtkallocatedbitmask.c'], ['-DGTK_COMPILATION', '-UG_ENABLE_DEBUG']],
  ['builder', [], [], gtk_tests_export_dynamic_ldflag],
  ['builderparser'],
  ['cellarea'],
  ['check-icon-names'],
  ['check-cursor-names'],
  ['cssprovider'],
  ['defaultvalue'],
  ['entry'],
  ['firefox-stylecontext'],
  ['floating'],
  ['focus'],
  ['gestures'],
  ['grid'],
  ['gtkmenu'],
  ['icontheme'],
  ['keyhash', ['../../gtk/gtkkeyhash.c', gtkresources, '../../gtk/gtkprivate.c'], gtk_cargs],
  ['listbox'],
  ['notify'],
  ['no-gtk-init'],
  ['object'],
  ['papersize'],
  ['rbtree', ['../../gtk/gtkrbtree.c'], ['-DGTK_COMPILATION', '-UG_ENABLE_DEBUG']],
  ['recentmanager'],
  ['regression-tests'],
  ['scrolledwindow'],
  ['spinbutton'],
  ['stylecontext'],
  ['templates'],
  ['textbuffer'],
  ['textiter'],
  ['treemodel', ['treemodel.c', 'liststore.c', 'treestore.c', 'filtermodel.c',
                 'modelrefcount.c', 'sortmodel.c', 'gtktreemodelrefcount.c']],
  ['treepath'],
  ['treeview'],
  ['typename'],
  ['window'],
  ['displayclose'],
  ['revealer-size'],
]

# Tests that are expected to fail, sometimes or always
flaky = [
]
xfail = [
]

is_debug = get_option('buildtype').startswith('debug')

test_cargs = []

if os_unix
  # tests += [['defaultvalue']]  # disabled in Makefile.am as well
  test_cargs += ['-DHAVE_UNIX_PRINT_WIDGETS']
endif

if x11_enabled
  tests += [
    ['clipboard'],
    ['objects-finalize', ['../../gdk/x11/gdkdisplaymanager-x11.c'], ['-DGDK_COMPILATION', '-UG_ENABLE_DEBUG']],
  ]
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
    install: get_option('installed_tests'),
    install_dir: installed_test_bindir)

  suites = ['gtk']

  if flaky.contains(test_name)
    suites += 'flaky'
  endif

  if xfail.contains(test_name)
    suites += 'failing'
  endif

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
              'GTK_TEST_MESON=1',
            ],
       suite: suites,
  )
endforeach

# FIXME: if objc autotestkeywords_CPPFLAGS += -DHAVE_OBJC=1 -x objective-c++
if add_languages('cpp', required: false, native: false)
  test_exe = executable('autotestkeywords',
                        'autotestkeywords.cc',
                        c_args : test_cargs + ['-Idummy-headers'],
                        dependencies : libgtk_dep,
                        install: get_option('installed_tests'),
                        install_dir: installed_test_bindir)
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

  if get_option('installed_tests')
    conf = configuration_data()
    conf.set('testexecdir', installed_test_bindir)
    conf.set('test', 'autotestkeywords')
    configure_file(input: 'gtk.test.in',
                   output: 'autotestkeywords.test',
                   configuration: conf,
                   install_dir: installed_test_datadir)
  endif
endif


if get_option('installed_tests')
  foreach t : tests
    test_name = t.get(0)

    if flaky.contains(test_name) or xfail.contains(test_name)
      continue
    endif

    conf = configuration_data()
    conf.set('testexecdir', installed_test_bindir)
    conf.set('test', test_name)
    configure_file(input: 'gtk.test.in',
                   output: '@0@.test'.format(test_name),
                   configuration: conf,
                   install_dir: installed_test_datadir)
  endforeach

  install_subdir('icons', install_dir: installed_test_bindir)
  install_subdir('icons2', install_dir: installed_test_bindir)
  install_subdir('ui', install_dir: installed_test_bindir)
endif
