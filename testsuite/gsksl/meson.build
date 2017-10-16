test_env = environment()
test_env.set('G_TEST_SRCDIR', meson.current_source_dir())
test_env.set('G_TEST_BUILDDIR', meson.current_build_dir())
test_env.set('GTK_IM_MODULE', 'gtk-im-context-simple')
test_env.set('GSETTINGS_BACKEND', 'memory')
test_env.set('G_ENABLE_DIAGNOSTIC', '0')

test_parser = executable('test-parser', 'test-parser.c', dependencies: libgtk_dep)
test('gsksl/parser', test_parser)

test_errors = executable('test-error', 'test-error.c', dependencies: libgtk_dep)
test('gsksl/error', test_errors)
