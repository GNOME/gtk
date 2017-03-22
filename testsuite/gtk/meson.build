tests = [
  ['accel'],
  ['accessible'],
  ['adjustment'],
  ['bitmask', ['../../gtk/gtkallocatedbitmask.c'], ['-DGTK_COMPILATION', '-UG_ENABLE_DEBUG']],
  ['builder', [], [], ['-export-dynamic']],
  ['builderparser'],
  ['cellarea'],
  ['check-icon-names'],
  ['check-cursor-names'],
  ['clipboard'],
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
]

test_env = environment()
test_env.set('G_TEST_SRCDIR', meson.current_source_dir())
test_env.set('G_TEST_BUILDDIR', meson.current_build_dir())
test_env.set('G_ENABLE_DIAGNOSTIC', '0')
test_env.set('GIO_USE_VFS', 'local')
test_env.set('GSETTINGS_BACKEND', 'memory')

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
    dependencies : libgtk_dep)

  test('@0@ test'.format(test_name), test_exe, suite : 'gtk', env : test_env)
endforeach

# FIXME: if objc autotestkeywords_CPPFLAGS += -DHAVE_OBJC=1 -x objective-c++
if add_languages('cpp')
  test_exe = executable('autotestkeywords',
    'autotestkeywords.cc',
    c_args : test_cargs + ['-Idummy-headers'],
    dependencies : libgtk_dep)
  test('autotestkeywords test', test_exe, suite : 'gtk', env : test_env)
endif

# FIXME: schemas from gtk+ dir?
# $(top_srcdir)/gtk/org.gtk.Settings.ColorChooser.gschema.xml
# $(top_srcdir)/gtk/org.gtk.Settings.FileChooser.gschema.xml
# gnome.compile_schemas()

# Installed data

test_icontheme = [
  'icons/index.theme',
  'icons/16x16/simple.png',
  'icons/16x16/twosize-fixed.svg',
  'icons/16x16s/twosize.svg',
  'icons/32x32/only32-symbolic.svg',
  'icons/32x32/twosize-fixed.svg',
  'icons/32x32s/twosize.svg',
  'icons/scalable/everything-justregular.svg',
  'icons/scalable/everything-justrtl-rtl.svg',
  'icons/scalable/everything-rtl.svg',
  'icons/scalable/everything-symbolic-rtl.svg',
  'icons/scalable/everything-justsymbolic-symbolic.svg',
  'icons/scalable/everything.svg',
  'icons/scalable/everything-symbolic.svg',
  'icons/scalable/nonsquare-symbolic.svg',
  'icons/15/size-test.png',
  'icons/16-22/size-test.png',
  'icons/25+/size-test.svg',
  'icons/35+/size-test.svg',
  'icons/scalable/one-two.svg',
  'icons/scalable/one-two-rtl.svg',
  'icons2/scalable/one-two-three-symbolic.svg',
  'icons2/scalable/one-two-symbolic.svg',
  'icons2/scalable/one-two-symbolic-rtl.svg',
  'icons2/index.theme',
]

test_ui = [
  'ui/test1.ui', 'ui/test1.expected',
  'ui/test2.ui', 'ui/test2.expected',
  'ui/test3.ui', 'ui/test3.expected',
  'ui/test4.ui', 'ui/test4.expected',
  'ui/test5.ui', 'ui/test5.expected',
  'ui/test6.ui', 'ui/test6.expected',
  'ui/test7.ui', 'ui/test7.expected',
  'ui/label1.ui', 'ui/label1.expected',
  'ui/label2.ui', 'ui/label2.expected',
  'ui/label3.ui', 'ui/label3.expected',
  'ui/label4.ui', 'ui/label4.expected',
  'ui/label5.ui', 'ui/label5.expected',
  'ui/celllayout1.ui', 'ui/celllayout1.expected',
  'ui/celllayout2.ui', 'ui/celllayout2.expected',
  'ui/celllayout3.ui', 'ui/celllayout3.expected',
  'ui/celllayout4.ui', 'ui/celllayout4.expected',
  'ui/celllayout5.ui', 'ui/celllayout5.expected',
  'ui/celllayout6.ui', 'ui/celllayout6.expected',
  'ui/celllayout7.ui', 'ui/celllayout7.expected',
  'ui/container1.ui', 'ui/container1.expected',
  'ui/container2.ui', 'ui/container2.expected',
  'ui/container3.ui', 'ui/container3.expected',
  'ui/container4.ui', 'ui/container4.expected',
  'ui/container5.ui', 'ui/container5.expected',
  'ui/container6.ui', 'ui/container6.expected',
  'ui/container7.ui', 'ui/container7.expected',
  'ui/container8.ui', 'ui/container8.expected',
  'ui/scale1.ui', 'ui/scale1.expected',
  'ui/scale2.ui', 'ui/scale2.expected',
  'ui/scale3.ui', 'ui/scale3.expected',
  'ui/scale4.ui', 'ui/scale4.expected',
  'ui/scale5.ui', 'ui/scale5.expected',
  'ui/scale6.ui', 'ui/scale6.expected',
  'ui/scale7.ui', 'ui/scale7.expected',
  'ui/levelbar1.ui', 'ui/levelbar1.expected',
  'ui/levelbar2.ui', 'ui/levelbar2.expected',
  'ui/levelbar3.ui', 'ui/levelbar3.expected',
  'ui/levelbar4.ui', 'ui/levelbar4.expected',
  'ui/levelbar5.ui', 'ui/levelbar5.expected',
  'ui/levelbar6.ui', 'ui/levelbar6.expected',
  'ui/dialog1.ui', 'ui/dialog1.expected',
  'ui/dialog2.ui', 'ui/dialog2.expected',
  'ui/dialog3.ui', 'ui/dialog3.expected',
  'ui/dialog4.ui', 'ui/dialog4.expected',
  'ui/dialog5.ui', 'ui/dialog5.expected',
  'ui/dialog6.ui', 'ui/dialog6.expected',
  'ui/filefilter1.ui', 'ui/filefilter1.expected',
  'ui/filefilter2.ui', 'ui/filefilter2.expected',
  'ui/filefilter3.ui', 'ui/filefilter3.expected',
  'ui/filefilter4.ui', 'ui/filefilter4.expected',
  'ui/filefilter5.ui', 'ui/filefilter5.expected',
  'ui/filefilter6.ui', 'ui/filefilter6.expected',
  'ui/filefilter7.ui', 'ui/filefilter7.expected',
  'ui/filefilter8.ui', 'ui/filefilter8.expected',
  'ui/filefilter9.ui', 'ui/filefilter9.expected',
  'ui/filefilter10.ui', 'ui/filefilter10.expected',
  'ui/infobar1.ui', 'ui/infobar1.expected',
  'ui/infobar2.ui', 'ui/infobar2.expected',
  'ui/infobar3.ui', 'ui/infobar3.expected',
  'ui/infobar4.ui', 'ui/infobar4.expected',
  'ui/infobar5.ui', 'ui/infobar5.expected',
  'ui/infobar6.ui', 'ui/infobar6.expected',
  'ui/image1.ui', 'ui/image1.expected',
  'ui/image2.ui', 'ui/image2.expected',
]

# TODO: installed tests + .test files
# if install_tests
#   install_data(test_icontheme + test_ui,
#     install_dir : join_paths(libexecdir, 'installed-tests/gtk+'))
# endif
