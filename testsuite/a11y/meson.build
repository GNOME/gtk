testexecdir = join_paths(installed_test_bindir, 'gtk')
testdatadir = join_paths(installed_test_datadir, 'gtk')

# Available keys for each test:
#
#  - 'name': the test name; used for the test and to determine the base
#            source file for the test (MANDATORY)
#  - 'sources': (array): additional sources needed by the test
#  - 'c_args': (array): additional compiler arguments
#  - 'link_args': (array): additional linker arguments
#  - 'suites': (array): additional test suites
tests = [
  { 'name': 'accessible' },
  { 'name': 'button' },
  { 'name': 'checkbutton' },
  { 'name': 'dialog' },
  { 'name': 'entry' },
  { 'name': 'expander' },
  { 'name': 'flowbox' },
  { 'name': 'general' },
  { 'name': 'image' },
  { 'name': 'label' },
  { 'name': 'listbox' },
  { 'name': 'levelbar' },
  { 'name': 'passwordentry' },
  { 'name': 'progressbar' },
  { 'name': 'scrollbar' },
  { 'name': 'searchentry' },
  { 'name': 'separator' },
  { 'name': 'spinbutton' },
  { 'name': 'stack' },
  { 'name': 'switch' },
  { 'name': 'textview' },
  { 'name': 'window' },
]


# Tests that are expected to fail
xfail = [
]

is_debug = get_option('buildtype').startswith('debug')

test_cargs = []

foreach flag: common_cflags
  if flag not in ['-Werror=missing-prototypes', '-Werror=missing-declarations', '-fvisibility=hidden']
    test_cargs += flag
  endif
endforeach

test_env = environment()
test_env.set('GTK_A11Y', 'test')
test_env.set('GSK_RENDERER', 'cairo')
test_env.set('G_TEST_SRCDIR', meson.current_source_dir())
test_env.set('G_TEST_BUILDDIR', meson.current_build_dir())
test_env.set('GIO_USE_VFS', 'local')
test_env.set('GSETTINGS_BACKEND', 'memory')
test_env.set('G_ENABLE_DIAGNOSTIC', '0')

foreach t : tests
  test_name = t.get('name')
  test_srcs = ['@0@.c'.format(test_name)] + t.get('sources', [])
  test_extra_cargs = t.get('c_args', [])
  test_extra_ldflags = t.get('link_args', [])
  test_extra_suites = t.get('suites', [])
  test_timeout = 60

  test_exe = executable(test_name, test_srcs,
    c_args: test_cargs + test_extra_cargs,
    link_args: test_extra_ldflags,
    dependencies: libgtk_dep,
    install: get_option('install-tests'),
    install_dir: testexecdir,
  )

  expect_fail = xfail.contains(test_name)

  if test_extra_suites.contains('slow')
    test_timeout = 90
  endif

  test(test_name, test_exe,
    args: [ '--tap', '-k' ],
    protocol: 'tap',
    timeout: test_timeout,
    env: test_env,
    suite: ['a11y'] + test_extra_suites,
    should_fail: expect_fail,
  )
endforeach
