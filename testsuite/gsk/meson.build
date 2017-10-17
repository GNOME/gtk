test_render_nodes = executable(
  'test-render-nodes',
  ['test-render-nodes.c',
   'reftest-compare.c'],
  dependencies: libgtk_dep,
)

cairo_test_env = environment()
cairo_test_env.set('G_TEST_SRCDIR', meson.current_source_dir())
cairo_test_env.set('G_TEST_BUILDDIR', meson.current_build_dir())
cairo_test_env.set('GSETTINGS_BACKEND', 'memory')
cairo_test_env.set('G_ENABLE_DIAGNOSTIC', '0')
cairo_test_env.set('GSK_RENDERER', 'cairo')

test('nodes (cairo)', test_render_nodes, suite: 'gsk', env: cairo_test_env)

vulkan_test_env = environment()
vulkan_test_env.set('G_TEST_SRCDIR', meson.current_source_dir())
vulkan_test_env.set('G_TEST_BUILDDIR', meson.current_build_dir())
vulkan_test_env.set('GSETTINGS_BACKEND', 'memory')
vulkan_test_env.set('G_ENABLE_DIAGNOSTIC', '0')
vulkan_test_env.set('GSK_RENDERER', 'vulkan')

test('nodes (vulkan)', test_render_nodes, suite: 'gsk', env: vulkan_test_env)

test_data = [
  'blendmode.node',
  'blendmode.png',
  'blendmodes.node',
  'blendmodes.png',
  'cairo.node',
  'cairo.png',
  'colors.cairo.png',
  'colors.node',
  'colors.vulkan.png',
  'cross-fade.node',
  'cross-fade.png',
  'cross-fades.node',
  'cross-fades.png',
  'ducky.png',
  'repeat.node',
  'repeat.png',
  'transform.node',
  'transform.png',
]

if get_option('install-tests')
  conf = configuration_data()
  conf.set('libexecdir', gtk_libexecdir)
  configure_file(input: 'test-render-nodes.test.in',
                 output: 'test-render-nodes.test',
                 configuration: conf,
                 install_dir: installed_test_datadir)

  install_data(test_data, install_dir: testexecdir)

endif

