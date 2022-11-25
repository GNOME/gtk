gtk_libexecdir = join_paths(gtk_prefix, get_option('libexecdir'))
installed_test_bindir = join_paths(gtk_libexecdir, 'installed-tests', 'gtk-4.0')
installed_test_datadir = join_paths(gtk_datadir, 'installed-tests', 'gtk-4.0')

common_env = [
  'GIO_USE_VOLUME_MONITOR=unix',
  'GSETTINGS_BACKEND=memory',
  'GTK_CSD=1',
  'G_ENABLE_DIAGNOSTIC=0',
  'GSETTINGS_SCHEMA_DIR=@0@'.format(gtk_schema_build_dir),
  'GDK_DEBUG=default-settings',
]
exclude_unstable = ['flaky', 'failing']

setups = [
  { 'backend': 'x11', 'if': x11_enabled, },
  { 'backend': 'wayland', 'if': wayland_enabled, 'is_default': true, },
  { 'name': 'waylandgles', 'backend': 'wayland', 'if': wayland_enabled,
    'env': ['GDK_DEBUG=gl-gles,default-settings'], },
  { 'backend': 'win32', 'if': os_win32 },
  { 'backend': 'broadway', 'if': broadway_enabled, },
]

foreach setup : setups
  if setup.get('if')
    backend = setup.get('backend')
    name = setup.get('name', backend)
    exclude = []

    if backend != 'broadway'
      exclude += 'gsk-compare-broadway'
    endif

    env = common_env + [
      'GDK_BACKEND=@0@'.format(backend),
    ] + setup.get('env', [])

    add_test_setup(
      name,
      env: env + ['TEST_OUTPUT_SUBDIR=@0@'.format(name)],
      exclude_suites: exclude_unstable + exclude,
      is_default: setup.get('is_default', false),
    )

    add_test_setup(
      '@0@_unstable'.format(name),
      env: env + ['TEST_OUTPUT_SUBDIR=@0@_unstable'.format(name)],
      exclude_suites: exclude,
    )
  endif
endforeach

subdir('performance')
subdir('gdk')
subdir('gsk')
subdir('gtk')
subdir('css')
subdir('a11y')
subdir('tools')
subdir('reftests')

if build_gir
  subdir('introspection')
endif
