clipboard_client = executable('clipboard-client',
  sources: ['clipboard-client.c'],
  include_directories: [confinc],
  c_args: common_cflags,
  dependencies: [ libgtk_dep ],
  install: false,
)

clipboard_test = executable('clipboard',
  sources: 'clipboard.c',
  c_args: common_cflags,
  dependencies: libgtk_dep,
  install: false,
)

test('clipboard', clipboard_test,
  args: [ '--tap', '-k' ],
  protocol: 'tap',
  is_parallel: false,
  env: [
      'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
      'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
      'DBUS_SESSION_BUS_ADDRESS=',
  ],
  suite: 'gdk',
)

test_sources = [
  'array.c',
  'cairo.c',
  'contentformats.c',
  'contentserializer.c',
  'cursor.c',
  'display.c',
  'displaymanager.c',
  'encoding.c',
  'glcontext.c',
  'keysyms.c',
  'memorytexture.c',
  'pixbuf.c',
  'rectangle.c',
  'rgba.c',
  'seat.c',
  'texture-threads.c',
  'gdktests.c'
]

gdktests = executable('gdktests',
  sources: test_sources,
  c_args: common_cflags,
  dependencies: libgtk_dep,
  install: false,
)

test('gdktests', gdktests,
  args: [ '--tap', '-k' ],
  protocol: 'tap',
  is_parallel: true,
  env: [
      'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
      'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
      'DBUS_SESSION_BUS_ADDRESS=',
  ],
  suite: 'gdk',
)

internal_test_sources = [
  'image.c',
  'texture.c',
  'gdkinternaltests.c',
]

gdkinternaltests = executable('gdkinternaltests',
  sources: internal_test_sources,
  c_args: common_cflags,
  dependencies: libgtk_static_dep,
  install: false,
)

test('gdkinternaltests', gdkinternaltests,
  args: [ '--tap', '-k' ],
  protocol: 'tap',
  env: [
      'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
      'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
      'DBUS_SESSION_BUS_ADDRESS=',
  ],
  suite: 'gdk',
)
