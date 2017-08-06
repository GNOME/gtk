bash = find_program('bash', required : false)
if bash.found()
  test_env = environment()
  test_env.set('GTK_BUILDER_TOOL', get_variable('gtk4_builder_tool').full_path())
  test_env.set('GTK_QUERY_SETTINGS', get_variable('gtk4_query_settings').full_path())

  foreach t : ['simplify', 'settings']
    configure_file(output : 'test-@0@'.format(t),
      input : 'test-@0@.in'.format(t),
      configuration : configuration_data())

    test('test-@0@'.format(t), bash,
      args : 'test-@0@'.format(t),
      workdir : meson.current_build_dir(),
      env : test_env)
  endforeach
endif # bash found

# Installed data

test_simplify_data = [
  'simplify/test1.ui', 'simplify/test1.expected',
  'simplify/test2.ui', 'simplify/test2.expected',
  'simplify/test3.ui', 'simplify/test3.expected',
  'simplify/test4.ui', 'simplify/test4.expected',
  'simplify/test5.ui', 'simplify/test5.expected',
  'simplify/test6.ui', 'simplify/test6.expected',
  'simplify/test7.ui', 'simplify/test7.expected',
  'simplify/test8.ui', 'simplify/test8.expected',
]
# TODO: install in case of installed tests - install_data()
