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
  ['outset shadow simple',         'outset_shadow_simple'],
  ['outset shadow offset x',       'outset_shadow_offsetx'],
  ['outset shadow offset y',       'outset_shadow_offsety'],
  ['outset shadow offset both',    'outset_shadow_offset_both'],
  ['outset shadow rounded1',       'outset_shadow_rounded1'],
  ['outset shadow rounded2',       'outset_shadow_rounded2'],
  ['outset shadow rounded top',    'outset_shadow_rounded_top'],
  ['outset shadow blurred simple', 'outset_shadow_blurred_simple'],
  ['outset shadow blurred small',  'outset_shadow_blurred_small'],
  ['outset shadow blurred offset', 'outset_shadow_blurred_offset'],
  ['outset shadow transformed',    'outset_shadow_translated'],
  ['Transform in Shadow',          'shadow_transform1'],
  ['Clipped crossfade',            'clipped_cross_fade'],
  ['Clipped rounded clip',         'clipped_rounded_clip'],
  ['clipped rounded clip2',        'transformed_clipped_rounded_clip'],
]

foreach gl_test : gl_tests
  test('GL ' + gl_test[0], compare_render,
       args: [join_paths(meson.current_source_dir(), 'gl', gl_test[1] + '.node'),
              join_paths(meson.current_source_dir(), 'gl', gl_test[1] + '.gl.png')],
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

