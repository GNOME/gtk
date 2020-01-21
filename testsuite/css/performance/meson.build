test_css_performance = executable('test-css-performance', 'test-css-performance.c',
                                  dependencies: [profiler_dep, platform_gio_dep, libm])

test('performance', test_css_performance,
     args: [ join_paths(meson.current_build_dir(), '../../../demos/widget-factory/gtk4-widget-factory') ],
     env: [ 'GTK_THEME=Adwaita' ],
     suite: [ 'css' ])
