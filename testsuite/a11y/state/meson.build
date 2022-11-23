a11y_state_record_bin = executable('state-record',
  'state-record.c',
  dependencies: libgtk_dep,
  install: get_option('installed_tests'),
  install_dir: installed_test_bindir,
)

test_data = [
  'focus1.in',
  'focus1.out',
  'focus1.ui',
  'focus2.in',
  'focus2.out',
  'focus2.ui',
]


a11y_installed_tests = [
  # This is not run at build time at all, and consistently fails on
  # Debian's CI infrastructure, so don't set it up to be run by
  # ginsttest-runner.
  #'a11ystate.test',
]

if get_option('installed_tests')
  test_cdata = configuration_data()
  test_cdata.set('libexecdir', gtk_libexecdir)

  foreach t: a11y_installed_tests
    configure_file(input: '@0@.in'.format(t),
                   output: t,
                   configuration: test_cdata,
                   install: true,
                   install_dir: installed_test_datadir)
  endforeach

  install_data(test_data, install_dir: join_paths(installed_test_bindir, 'state'))
endif
