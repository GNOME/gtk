testexecdir = join_paths(installed_test_bindir, 'gdk')
testdatadir = join_paths(installed_test_datadir, 'gdk')

clipboard_client = executable('clipboard-client',
                              sources: ['clipboard-client.c'],
                              include_directories: [confinc],
                              c_args: common_cflags,
                              dependencies: [ libgtk_dep ],
                              install: get_option('install-tests'),
                              install_dir: testexecdir)

tests = [
  'array',
  'cairo',
  'clipboard',
  'contentserializer',
  'cursor',
  'display',
  'displaymanager',
  'encoding',
  'keysyms',
  'memorytexture',
  'pixbuf',
  'rectangle',
  'rgba',
  'seat',
  'texture',
]

foreach t : tests
  test_exe = executable(t, '@0@.c'.format(t),
    c_args: common_cflags,
    dependencies: libgtk_dep,
    install: get_option('install-tests'),
    install_dir: testexecdir,
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

if get_option('install-tests')
  foreach t : tests
    test_cdata = configuration_data()
    test_cdata.set('testexecdir', testexecdir)
    test_cdata.set('test', t)
    configure_file(input: 'gdk.test.in',
      output: '@0@.test'.format(t),
      configuration: test_cdata,
      install: true,
      install_dir: testdatadir,
    )
  endforeach

  install_subdir('clipboard-data', install_dir: testexecdir)
endif
