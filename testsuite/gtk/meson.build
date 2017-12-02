testexecdir = join_paths(installed_test_bindir, 'gtk')
testdatadir = join_paths(installed_test_datadir, 'gtk')

gtk_tests_export_dynamic_ldflag = []

if cc.get_id() != 'msvc'
  gtk_tests_export_dynamic_ldflag = ['-Wl,--export-dynamic']
endif

tests = [
  ['accel'],
  ['accessible'],
  ['adjustment'],
  ['bitmask', ['../../gtk/gtkallocatedbitmask.c'], ['-DGTK_COMPILATION', '-UG_ENABLE_DEBUG']],
  ['builder', [], [], gtk_tests_export_dynamic_ldflag],
  ['builderparser'],
  ['cellarea'],
  ['check-icon-names'],
  ['cssprovider'],
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
  ['objects-finalize'],
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
              'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir())
            ],
       suite: 'gtk')
endforeach

# FIXME: if objc autotestkeywords_CPPFLAGS += -DHAVE_OBJC=1 -x objective-c++
if add_languages('cpp')
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
              'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir())
            ],
       suite: 'gtk')
  if get_option('install-tests')
    conf = configuration_data()
    conf.set('testexecdir', testexecdir)
    conf.set('test', 'keywords')
    configure_file(input: 'gtk.test.in',
                   output: 'keywords.test',
                   configuration: conf,
                   install_dir: testdatadir)
  endif
endif

# FIXME: schemas from gtk+ dir?
# $(top_srcdir)/gtk/org.gtk.Settings.ColorChooser.gschema.xml
# $(top_srcdir)/gtk/org.gtk.Settings.FileChooser.gschema.xml
# gnome.compile_schemas()

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

