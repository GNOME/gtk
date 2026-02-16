svgtest_env = environment()
svgtest_env.set('G_TEST_SRCDIR', meson.current_source_dir())
svgtest_env.set('G_TEST_BUILDDIR', meson.current_build_dir())

subdir('parser')
subdir('renderer')
subdir('animation')

internal_tests = [
  'path-classify'
]

foreach t : internal_tests
  test_name = t
  test_srcs = ['@0@.c'.format(test_name)]
  test_exe = executable(test_name, test_srcs,
    dependencies : libgtk_static_dep,
    c_args : test_cargs + common_cflags + ['-DGTK_COMPILATION'],
  )

  test(test_name, test_exe,
    args: [ '--tap', '-k' ],
    protocol: 'tap',
    env: [
      'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
      'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir())
    ],
    suite: [ 'svg' ],
  )
endforeach

