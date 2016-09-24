test_accel = executable('accel', 'accel.c', dependencies: libgtk_dep)
test('gtk/accel test', test_accel)

test_accessible = executable('accessible', 'accessible.c', dependencies: libgtk_dep)
test('gtk/accessible test', test_accessible)

test_action = executable('action', 'action.c', dependencies: libgtk_dep)
test('gtk/action test', test_action)


test_adjustment = executable('adjustment', 'adjustment.c', dependencies: libgtk_dep)
test('gtk/adjustment test', test_adjustment)

test_bitmask = executable('bitmask', 'bitmask.c', dependencies: libgtk_dep)
test('gtk/bitmask test', test_bitmask)

test_builder = executable('builder', 'builder.c', dependencies: libgtk_dep)
test('gtk/builder test', test_builder)

test_builderparser = executable(
  'builderparser',
  'builderparser.c',
  dependencies: libgtk_dep,
  link_with: libgtk,

)
test(
  'gtk/builderparser test',
  test_builderparser,
  env: [
      'G_TEST_SRCDIR='+meson.source_root(),
      'G_TEST_BUILDDIR='+meson.build_root()
  ]
)

test_cellarea = executable('cellarea', 'cellarea.c', dependencies: libgtk_dep)
test('gtk/cellarea test', test_cellarea)

test_check_icon_names = executable('check_icon_names', 'check-icon-names.c', dependencies: libgtk_dep)
test('gtk/check_icon_names test', test_check_icon_names)

test_check_cursor_names = executable('check_cursor_names', 'check-cursor-names.c', dependencies: libgtk_dep)
test('gtk/check_cursor_names test', test_check_cursor_names)

test_clipboard = executable('clipboard', 'clipboard.c', dependencies: libgtk_dep)
test('gtk/clipboard test', test_clipboard)

test_cssprovider = executable('cssprovider', 'cssprovider.c', dependencies: libgtk_dep)
test('gtk/cssprovider test', test_cssprovider)

test_defaultvalue = executable('defaultvalue', 'defaultvalue.c', dependencies: libgtk_dep)
test('gtk/defaultvalue test', test_defaultvalue)

test_entry = executable('entry', 'entry.c', dependencies: libgtk_dep)
test('gtk/entry test', test_entry)

test_firefox_stylecontext = executable('firefox_stylecontext', 'firefox-stylecontext.c', dependencies: libgtk_dep)
test('gtk/firefox_stylecontext test', test_firefox_stylecontext)

test_floating = executable('floating', 'floating.c', dependencies: libgtk_dep)
test('gtk/floating test', test_floating)

test_focus = executable('focus', 'focus.c', dependencies: libgtk_dep)
test('gtk/focus test', test_focus)

test_gestures = executable('gestures', 'gestures.c', dependencies: libgtk_dep)
test('gtk/gestures test', test_gestures)

test_grid = executable('grid', 'grid.c', dependencies: libgtk_dep)
test('gtk/grid test', test_grid)

test_gtkmenu = executable('gtkmenu', 'gtkmenu.c', dependencies: libgtk_dep)
test('gtk/gtkmenu test', test_gtkmenu)

test_icontheme = executable('icontheme', 'icontheme.c', dependencies: libgtk_dep)
test('gtk/icontheme test', test_icontheme)

test_keyhash = executable('keyhash', 'keyhash.c', dependencies: libgtk_dep)
test('gtk/keyhash test', test_keyhash)

test_listbox = executable('listbox', 'listbox.c', dependencies: libgtk_dep)
test('gtk/listbox test', test_listbox)

test_notify = executable('notify', 'notify.c', dependencies: libgtk_dep)
test('gtk/notify test', test_notify)

test_no_gtk_init = executable('no_gtk_init', 'no-gtk-init.c', dependencies: libgtk_dep)
test('gtk/no_gtk_init test', test_no_gtk_init)

test_object = executable('object', 'object.c', dependencies: libgtk_dep)
test('gtk/object test', test_object)

test_objects_finalize = executable('objects_finalize', 'objects-finalize.c', dependencies: libgtk_dep)
test('gtk/objects_finalize test', test_objects_finalize)

test_papersize = executable('papersize', 'papersize.c', dependencies: libgtk_dep)
test('gtk/papersize test', test_papersize)

test_rbtree = executable('rbtree', 'rbtree.c', dependencies: libgtk_dep)
test('gtk/rbtree test', test_rbtree)

test_recentmanager = executable('recentmanager', 'recentmanager.c', dependencies: libgtk_dep)
test('gtk/recentmanager test', test_recentmanager)

test_regression_tests = executable('regression_tests', 'regression-tests.c', dependencies: libgtk_dep)
test('gtk/regression_tests test', test_regression_tests)

test_scrolledwindow = executable('scrolledwindow', 'scrolledwindow.c', dependencies: libgtk_dep)
test('gtk/scrolledwindow test', test_scrolledwindow)

test_spinbutton = executable('spinbutton', 'spinbutton.c', dependencies: libgtk_dep)
test('gtk/spinbutton test', test_spinbutton)

test_stylecontext = executable('stylecontext', 'stylecontext.c', dependencies: libgtk_dep)
test('gtk/stylecontext test', test_stylecontext)

test_templates = executable('templates', 'templates.c', dependencies: libgtk_dep)
test('gtk/templates test', test_templates)

test_textbuffer = executable('textbuffer', 'textbuffer.c', dependencies: libgtk_dep)
test('gtk/textbuffer test', test_textbuffer)

test_textiter = executable('textiter', 'textiter.c', dependencies: libgtk_dep)
test('gtk/textiter test', test_textiter)

test_treemodel = executable(
  'treemodel',
  'treemodel.c', 'liststore.c', 'treestore.c', 'filtermodel.c',
  'modelrefcount.c', 'sortmodel.c', 'gtktreemodelrefcount.c',
  dependencies: libgtk_dep
)
test('gtk/treemodel test', test_treemodel)

test_treepath = executable('treepath', 'treepath.c', dependencies: libgtk_dep)
test('gtk/treepath test', test_treepath)

test_treeview = executable('treeview', 'treeview.c', dependencies: libgtk_dep)
test('gtk/treeview test', test_treeview)

test_typename = executable('typename', 'typename.c', dependencies: libgtk_dep)
test('gtk/typename test', test_typename)

test_window = executable('window', 'window.c', dependencies: libgtk_dep)
test('gtk/window test', test_window)

test_displayclose = executable('displayclose', 'displayclose.c', dependencies: libgtk_dep)
test('gtk/displayclose test', test_displayclose)

test_revealer_size = executable('revealer_size', 'revealer-size.c', dependencies: libgtk_dep)
test('gtk/revealer_size test', test_revealer_size)
