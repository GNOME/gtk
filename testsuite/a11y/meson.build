testexecdir = join_paths(installed_test_bindir, 'a11y')
testdatadir = join_paths(installed_test_datadir, 'a11y')

a11y_state_tests = [
  'about',
  'accessible-name',
  'actionbar',
  'assistant',
  #'buttons',
  'calendar',
  'colorchooser',
  'combos',
  'entries',
  'expander',
  'headerbar',
  'hello-world',
  'iconview',
  'infobar',
  'label',
  'link',
  'listbox',
  'lockbutton',
  'menubutton',
  'menubutton2',
  'messagedialog',
  'mnemonic',
  'notebook',
  'paned',
  'pickers',
  'placeholder-text',
  'progress',
  'range',
  'scale-drawvalue',
  'spinner',
  'statusbar',
  'stack',
  'text',
  'tooltips',
  'tree',
]

a11y_dump_bin = executable('accessibility-dump',
  'accessibility-dump.c',
  c_args: common_cflags,
  dependencies: libgtk_dep,
  install: get_option('install-tests'),
  install_dir: testexecdir,
)

foreach t: a11y_state_tests
  ui_path = join_paths(meson.current_source_dir(), t + '.ui')
  test (t + ' state',
    a11y_dump_bin,
    args: [ '--tap', '-k', ui_path ],
    protocol: 'tap',
    env: [
      'GIO_USE_VOLUME_MONITOR=unix',
      'GSETTINGS_BACKEND=memory',
      'GDK_DEBUG=default-settings',
      'GTK_CSD=1',
      'G_ENABLE_DIAGNOSTIC=0',
      'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
      'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
      'GSETTINGS_SCHEMA_DIR=@0@'.format(gtk_schema_build_dir),
    ],
    suite: 'a11y',
  )
endforeach

a11y_tests = [
  'tree-performance',
  'text',
  'children',
  'derive',
  'value',
  'misc',
  'tree-relationships',
  'util',
]

foreach t: a11y_tests
  test_prefix = 'a11y'

  test (t,
        executable (t, '@0@.c'.format(t),
                    install: get_option('install-tests'),
                    install_dir: testexecdir,
                    dependencies: libgtk_dep),
        args: [ '--tap', '-k', ],
        protocol: 'tap',
        timeout: 120,
        env: [ 'GIO_USE_VOLUME_MONITOR=unix',
               'GSETTINGS_BACKEND=memory',
               'GTK_CSD=1',
               'G_ENABLE_DIAGNOSTIC=0',
               'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
               'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
               'GSETTINGS_SCHEMA_DIR=@0@'.format(gtk_schema_build_dir),
             ],
        suite: 'a11y')
endforeach

a11y_installed_tests = [
  'tests.test',
  'children.test',
  'tree.test',
  'value.test',
  'derive.test',
  'text.test',
  'util.test',
  'misc.test',
]

installed_test_data = [
  'about.ui',             'about.txt',
  'accessible-name.ui',   'accessible-name.txt',
  'actionbar.ui',         'actionbar.txt',
  'assistant.ui',         'assistant.txt',
  'buttons.ui',           'buttons.txt',
  'calendar.ui',          'calendar.txt',
  'colorchooser.ui',      'colorchooser.txt',
  'combos.ui',            'combos.txt',
  'entries.ui',           'entries.txt',
  'expander.ui',          'expander.txt',
  'headerbar.ui',         'headerbar.txt',
  'hello-world.ui',       'hello-world.txt',
  'iconview.ui',          'iconview.txt',
  'infobar.ui',           'infobar.txt',
  'label.ui',             'label.txt',
  'link.ui',              'link.txt',
  'listbox.ui',           'listbox.txt',
  'lockbutton.ui',        'lockbutton.txt',
  'menubutton.ui',        'menubutton.txt',
  'menubutton2.ui',       'menubutton2.txt',
  'messagedialog.ui',     'messagedialog.txt',
  'mnemonic.ui',          'mnemonic.txt',
  'notebook.ui',          'notebook.txt',
  'paned.ui',             'paned.txt',
  'pickers.ui',           'pickers.txt',
  'placeholder-text.ui',  'placeholder-text.txt',
  'progress.ui',          'progress.txt',
  'range.ui',             'range.txt',
  'scale-drawvalue.ui',   'scale-drawvalue.txt',
  'spinner.ui',           'spinner.txt',
  'stack.ui',             'stack.txt',
  'statusbar.ui',         'statusbar.txt',
  'text.ui',              'text.txt',
  'tooltips.ui',          'tooltips.txt',
  'tree.ui',              'tree.txt',
]

if get_option('install-tests')
  test_cdata = configuration_data()
  test_cdata.set('libexecdir', gtk_libexecdir)

  foreach t: a11y_installed_tests
    configure_file(input: '@0@.in'.format(t),
                   output: t,
                   configuration: test_cdata,
                   install: true,
                   install_dir: testdatadir)
  endforeach

  install_data(installed_test_data, install_dir: testexecdir)
endif
