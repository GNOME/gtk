tests = [
  'cairo',
  # 'check-gdk-cairo' # disabled in Makefile.am
  'cursor',
  'display',
  'encoding',
  'keysyms',
  'rectangle',
  'rgba',
  'seat',
]

test_env = environment()
test_env.set('G_TEST_SRCDIR', meson.current_source_dir())
test_env.set('G_TEST_BUILDDIR', meson.current_build_dir())

foreach t : tests
  test_exe = executable(t, '@0@.c'.format(t), dependencies : libgtk_dep)

  test('@0@ test'.format(t), test_exe, suite : 'gdk', env : test_env)
endforeach

# TODO: installed tests + .test files
