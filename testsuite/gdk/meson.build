clipboard_client = executable('clipboard-client',
  sources: ['clipboard-client.c'],
  include_directories: [confinc],
  c_args: common_cflags,
  dependencies: [ libgtk_dep ],
  install: false,
)

tests = [
  { 'name': 'array' },
  { 'name': 'cairo' },
  { 'name': 'clipboard', 'parallel': false, },
  { 'name': 'contentformats' },
  { 'name': 'contentserializer' },
  { 'name': 'cursor' },
  { 'name': 'display' },
  { 'name': 'displaymanager' },
  { 'name': 'encoding' },
  { 'name': 'glcontext' },
  { 'name': 'keysyms' },
  { 'name': 'memorytexture' },
  { 'name': 'pixbuf' },
  { 'name': 'rectangle' },
  { 'name': 'rgba' },
  { 'name': 'seat' },
  { 'name': 'texture-threads' },
]

foreach t : tests
  test_name = t.get('name')
  test_exe = executable(test_name,
    sources: '@0@.c'.format(test_name),
    c_args: common_cflags,
    dependencies: libgtk_dep,
    install: false,
  )

  suites = ['gdk'] + t.get('suites', [])

  test(test_name, test_exe,
    args: [ '--tap', '-k' ],
    protocol: 'tap',
    is_parallel: t.get('parallel', false),
    env: [
      'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
      'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
      'DBUS_SESSION_BUS_ADDRESS=',
    ],
    suite: suites,
  )
endforeach

internal_tests = [
  'image',
  'texture',
]

foreach t : internal_tests
  test_exe = executable(t, '@0@.c'.format(t),
    c_args: common_cflags,
    dependencies: libgtk_static_dep,
    install: false,
  )

  test(t, test_exe,
    args: [ '--tap', '-k' ],
    protocol: 'tap',
    env: [
      'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
      'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
      'DBUS_SESSION_BUS_ADDRESS=',
    ],
    suite: 'gdk',
  )
endforeach
