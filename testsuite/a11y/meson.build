a11y_state_tests = [
  'hello-world',
  'mnemonic',
  'accessible-name',
  'notebook',
  'range',
  'link',
  'text',
  'buttons',
  'colorchooser',
  'about',
  'messagedialog',
  'expander',
  'assistant',
  'pickers',
  'label',
  'lockbutton',
  'spinner',
  'progress',
  'infobar',
  'calendar',
  'statusbar',
  'paned',
  'iconview',
  'entries',
  'scale-drawvalue',
  'placeholder-text',
  'menu',
  'menubutton',
  'menubutton2',
  'menubutton3',
  'combos',
  'listbox',
  'stack',
  'headerbar',
  'tree',
  'actionbar',
  'tooltips',
]

skipped_tests = [
  # gtk_color_picker_new() in CI returns NULL which changes the UI
  'colorchooser',
]

a11y_dump_bin = executable('accessibility-dump',
  'accessibility-dump.c',
  dependencies: libgtk_dep,
  install: get_option('installed_tests'),
  install_dir: installed_test_bindir,
)

foreach t: a11y_state_tests
  if t not in skipped_tests
    ui_path = join_paths(meson.current_source_dir(), t + '.ui')
    test (t + ' state',
      a11y_dump_bin,
      args: [ '--tap', '-k', ui_path ],
      env: [
        'GIO_USE_VOLUME_MONITOR=unix',
        'GSETTINGS_BACKEND=memory',
        'GTK_CSD=0',
        'G_ENABLE_DIAGNOSTIC=0',
        'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
        'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
        'GSETTINGS_SCHEMA_DIR=@0@'.format(gtk_schema_build_dir),
        'GTK_TEST_MESON=1',
      ],
      suite: ['a11y', 'flaky'],
    )
  endif
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
                    install: get_option('installed_tests'),
                    install_dir: installed_test_bindir,
                    dependencies: libgtk_dep),
        args: [ '--tap', '-k', ],
        timeout: 120,
        env: [ 'GIO_USE_VOLUME_MONITOR=unix',
               'GSETTINGS_BACKEND=memory',
               'GTK_CSD=1',
               'G_ENABLE_DIAGNOSTIC=0',
               'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
               'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
               'GSETTINGS_SCHEMA_DIR=@0@'.format(gtk_schema_build_dir),
             ],
        suite: ['a11y'])
endforeach

installed_test_data = [
  'hello-world.ui',
  'hello-world.txt',
  'mnemonic.ui',
  'mnemonic.txt',
  'accessible-name.ui',
  'accessible-name.txt',
  'notebook.ui',
  'notebook.txt',
  'range.ui',
  'range.txt',
  'link.ui',
  'link.txt',
  'text.ui',
  'text.txt',
  'buttons.ui',
  'buttons.txt',
  'colorchooser.ui',
  'colorchooser.txt',
  'about.ui',
  'about.txt',
  'messagedialog.ui',
  'messagedialog.txt',
  'expander.ui',
  'expander.txt',
  'assistant.ui',
  'assistant.txt',
  'pickers.ui',
  'pickers.txt',
  'label.ui',
  'label.txt',
  'lockbutton.ui',
  'lockbutton.txt',
  'spinner.ui',
  'spinner.txt',
  'progress.ui',
  'progress.txt',
  'infobar.ui',
  'infobar.txt',
  'calendar.ui',
  'calendar.txt',
  'statusbar.ui',
  'statusbar.txt',
  'paned.ui',
  'paned.txt',
  'iconview.ui',
  'iconview.txt',
  'entries.ui',
  'entries.txt',
  'scale-drawvalue.ui',
  'scale-drawvalue.txt',
  'placeholder-text.ui',
  'placeholder-text.txt',
  'menu.ui',
  'menu.txt',
  'menubutton.ui',
  'menubutton.txt',
  'menubutton2.ui',
  'menubutton2.txt',
  'menubutton3.ui',
  'menubutton3.txt',
  'combos.ui',
  'combos.txt',
  'listbox.ui',
  'listbox.txt',
  'stack.ui',
  'stack.txt',
  'headerbar.ui',
  'headerbar.txt',
  'tree.ui',
  'tree.txt',
  'actionbar.ui',
  'actionbar.txt',
  'tooltips.ui',
  'tooltips.txt',
]

a11y_installed_tests = [
  # This is the equivalent of a11y_state_tests above, and does not seem
  # to be reliable enough to act as a QA gate in practice. We install the
  # test executable and the data needed to run it, but don't hook it up
  # to ginsttest-runner.
  #'a11ytests.test',

  'a11ychildren.test',
  'a11ytree.test',
  'a11yvalue.test',
  'a11yderive.test',
  'a11ytext.test',
  'a11yutil.test',
  'a11ymisc.test',
]

if get_option('installed_tests')
  test_cdata = configuration_data()
  test_cdata.set('libexecdir', gtk_libexecdir)

  foreach t: a11y_installed_tests
    configure_file(input: '@0@.in'.format(t),
                   output: t,
                   configuration: test_cdata,
                   install_dir: installed_test_datadir)
  endforeach

  install_data(installed_test_data, install_dir: join_paths(installed_test_bindir, 'a11ytests'))
endif

subdir('state')
