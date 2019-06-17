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
      suite: 'a11y',
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
        suite: 'a11y')
endforeach

subdir('state')
