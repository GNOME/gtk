testexecdir = join_paths(installed_test_bindir, 'tools')
testdatadir = join_paths(installed_test_datadir, 'tools')

bash = find_program('bash', required : false)
if bash.found()
  test_env = environment()

  foreach t : ['simplify', 'settings']
    configure_file(output: t,
                   input: '@0@.in'.format(t),
                   configuration: configuration_data(),
                   install: get_option('install-tests'),
                   install_dir: testexecdir)
    test(t, bash,
         args: t,
         workdir: meson.current_build_dir(),
         env: [ 'GIO_USE_VOLUME_MONITOR=unix',
                'GSETTINGS_BACKEND=memory',
                'GTK_CSD=1',
                'G_ENABLE_DIAGNOSTIC=0',
                'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
                'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
                'GTK_BUILDER_TOOL=@0@'.format(get_variable('gtk4_builder_tool').full_path()),
                'GTK_QUERY_SETTINGS=@0@'.format(get_variable('gtk4_query_settings').full_path())
              ],
         suite: 'tools')
  endforeach
endif

if get_option('install-tests')
  foreach t : ['simplify', 'settings']
    test_conf = configuration_data()
    test_conf.set('testexecdir', testexecdir)
    test_conf.set('test', t)
    configure_file(output: '@0@.test'.format(t),
                   input: 'tools.test.in',
                   configuration: test_conf,
                   install: true,
                   install_dir: testdatadir)
  endforeach

  install_subdir('simplify-data', install_dir: testexecdir)
endif
