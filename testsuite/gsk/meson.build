testexecdir = join_paths(installed_test_bindir, 'gsk')
testdatadir = join_paths(installed_test_datadir, 'gsk')

test_render_nodes = executable(
  'test-render-nodes',
  ['test-render-nodes.c',
   'reftest-compare.c'],
  dependencies: libgtk_dep,
  install: get_option('install-tests'),
  install_dir: testexecdir
)

compare_render = executable(
  'compare-render',
  ['compare-render.c', 'reftest-compare.c'],
  dependencies: libgtk_dep,
  install: get_option('install-tests'),
  install_dir: testexecdir
)

test('nodes (cairo)', test_render_nodes,
     args: [ '--tap', '-k' ],
     env: [ 'GIO_USE_VOLUME_MONITOR=unix',
            'GSETTINGS_BACKEND=memory',
            'GTK_CSD=1',
            'G_ENABLE_DIAGNOSTIC=0',
            'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
            'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
            'GSK_RENDERER=cairo'
          ],
     suite: 'gsk')

# Interesting render nodes proven to be rendered 'correctly' by the GL renderer.
gl_tests = [
  ['outset shadow simple', 'outset_shadow_simple.node', 'outset_shadow_simple.gl.png'],
  ['outset shadow offset x', 'outset_shadow_offsetx.node', 'outset_shadow_offsetx.gl.png'],
  ['outset shadow offset y', 'outset_shadow_offsety.node', 'outset_shadow_offsety.gl.png'],
  ['outset shadow offset both', 'outset_shadow_offset_both.node', 'outset_shadow_offset_both.gl.png'],
  ['outset shadow rounded1', 'outset_shadow_rounded1.node', 'outset_shadow_rounded1.gl.png'],
  ['outset shadow rounded2', 'outset_shadow_rounded2.node', 'outset_shadow_rounded2.gl.png'],
  ['outset shadow rounded top', 'outset_shadow_rounded_top.node', 'outset_shadow_rounded_top.gl.png'],
  ['outset shadow blurred simple', 'outset_shadow_blurred_simple.node', 'outset_shadow_blurred_simple.gl.png'],
  ['outset shadow blurred small', 'outset_shadow_blurred_small.node', 'outset_shadow_blurred_small.gl.png'],
  ['outset shadow blurred offset', 'outset_shadow_blurred_offset.node', 'outset_shadow_blurred_offset.gl.png'],
  ['Text with alpha', 'text_alpha.node', 'text_alpha.gl.png']
]

foreach gl_test : gl_tests
  test('GL ' + gl_test[0], compare_render,
       args: [join_paths(meson.current_source_dir(), 'gl', gl_test[1]),
              join_paths(meson.current_source_dir(), 'gl', gl_test[2])],
       env: [ 'GIO_USE_VOLUME_MONITOR=unix',
              'GSETTINGS_BACKEND=memory',
              'GTK_CSD=1',
              'G_ENABLE_DIAGNOSTIC=0',
              'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
              'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
              'GSK_RENDERER=opengl'
            ],
       suite: 'gsk')
endforeach

if have_vulkan
  test('nodes (vulkan)', test_render_nodes,
       args: [ '--tap', '-k' ],
       env: [ 'GIO_USE_VOLUME_MONITOR=unix',
              'GSETTINGS_BACKEND=memory',
              'GTK_CSD=1',
              'G_ENABLE_DIAGNOSTIC=0',
              'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
              'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
              'GSK_RENDERER=vulkan'
            ],
       suite: 'gsk')
endif

test_data = [
  'blendmode.node',
  'blendmode.png',
  'blendmode.vulkan.png',
  'blendmodes.node',
  'blendmodes.png',
  'blendmodes.vulkan.png',
  'cairo.node',
  'cairo.png',
  'colors.cairo.png',
  'colors.node',
  'colors.vulkan.png',
  'cross-fade.node',
  'cross-fade.png',
  'cross-fades.node',
  'cross-fades.png',
  'cross-fades.vulkan.png',
  'ducky.png',
  'repeat.node',
  'repeat.png',
  'transform.node',
  'transform.png',
  'transform.vulkan.png',
  'opacity.node',
  'opacity.png',
  'opacity.vulkan.png',
]

if get_option('install-tests')
  conf = configuration_data()
  conf.set('libexecdir', gtk_libexecdir)
  configure_file(input: 'render-nodes-cairo.test.in',
                 output: 'render-nodes-cairo.test',
                 configuration: conf,
                 install_dir: testdatadir)
  configure_file(input: 'render-nodes-vulkan.test.in',
                 output: 'render-nodes-vulkan.test',
                 configuration: conf,
                 install_dir: testdatadir)

  install_data(test_data, install_dir: testexecdir)

endif

