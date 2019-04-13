bash = find_program('bash', required : false)
if bash.found()
  test_env = environment()

  foreach t : ['test-simplify', 'test-settings']
    configure_file(output: t,
                   input: '@0@.in'.format(t),
                   copy: true)
    test(t, bash,
         args: t,
         workdir: meson.current_build_dir(),
         env: [ 'GIO_USE_VOLUME_MONITOR=unix',
                'GSETTINGS_BACKEND=memory',
                'GTK_CSD=1',
                'G_ENABLE_DIAGNOSTIC=0',
                'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
                'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
                'GTK_BUILDER_TOOL=@0@'.format(gtk_builder_tool.full_path()),
                'GTK_QUERY_SETTINGS=@0@'.format(gtk_query_settings.full_path())
              ],
         suite: 'tools')
  endforeach
endif
