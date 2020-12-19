testexecdir = join_paths(installed_test_bindir, 'gsk')
testdatadir = join_paths(installed_test_datadir, 'gsk')

snapshot = executable(
  'snapshot',
  ['snapshot.c'],
  dependencies: libgtk_dep,
  c_args: common_cflags,
  install: get_option('install-tests'),
  install_dir: testexecdir
)

snapshot_tests = [
  [ 'trim-equal-is-empty', 0 ],
  [ 'trim-0-to-100-shows-everything', 0 ],
  [ 'trim-0-to-200-shows-everything-once', 0 ],
  [ 'trim-startend-does-minmax', 0 ],
  [ 'trim-offset-rotates', 0 ],
]

foreach test : snapshot_tests
  test('snapshot-' + test[0], snapshot,
    args: [
      '--time=' + test[1].to_string(),
      join_paths(meson.current_source_dir(), 'snapshot', test[0] + '.lottie'),
      join_paths(meson.current_source_dir(), 'snapshot', test[0] + '.' + test[1].to_string() + '.node'),
    ],
    env: [
      'GTK_A11Y=test',
      'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
      'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir())
    ],
    suite: [ 'ottie'],
  )
endforeach
