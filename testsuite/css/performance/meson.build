if get_option ('profiler')

  test('performance-adwaita', test_performance,
       args: [ '--mark', 'style', join_paths(meson.current_build_dir(), '../../../demos/widget-factory/gtk4-widget-factory') ],
       env: [ 'GTK_THEME=Adwaita',
              'GSETTINGS_SCHEMA_DIR=@0@'.format(gtk_schema_build_dir) ],
       suite: [ 'css' ])

  test('performance-empty', test_performance,
       args: [ '--mark', 'style', join_paths(meson.current_build_dir(), '../../../demos/widget-factory/gtk4-widget-factory') ],
       env: [ 'GTK_THEME=Empty',
              'GSETTINGS_SCHEMA_DIR=@0@'.format(gtk_schema_build_dir) ],
       suite: [ 'css' ])

endif
