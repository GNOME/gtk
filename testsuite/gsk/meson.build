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

node_parser = executable(
  'node-parser',
  ['node-parser.c'],
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
  ['outset shadow rounded1',       'outset_shadow_rounded1'],
  ['outset shadow rounded2',       'outset_shadow_rounded2'],
  ['outset shadow blurred simple', 'outset_shadow_blurred_simple'],
  ['outset shadow blurred small',  'outset_shadow_blurred_small'],
  ['outset shadow blurred offset', 'outset_shadow_blurred_offset'],
  ['Crossfade simple',             'cross_fade'],
  ['gradient simple',              'gradient_simple'],
  ['gradient clipped',             'gradient_clipped'],
]

foreach gl_test : gl_tests
  test('GL ' + gl_test[0], compare_render,
       args: [join_paths(meson.current_source_dir(), 'gl', gl_test[1] + '.txt'),
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

compare_render_tests = [
  'clip-coordinates-3d',
  'clipped_rounded_clip',
  'opacity_clip',
  'outset_shadow_offset_both',
  'outset_shadow_offset_x',
  'outset_shadow_offset_y',
  'outset_shadow_rounded_top',
  'outset_shadow_simple',
]

renderers = [
  # name      exclude term
  [ 'opengl', ''    ],
  [ 'cairo',  '-3d' ],
]

foreach renderer : renderers
  foreach test : compare_render_tests
    if (renderer[1] != '' and test.contains(renderer[1]))
      continue
    endif
    test(renderer[0] + ' ' + test, compare_render,
         args: [join_paths(meson.current_source_dir(), 'compare', test + '.node'),
                join_paths(meson.current_source_dir(), 'compare', test + '.png')],
         env: [ 'GIO_USE_VOLUME_MONITOR=unix',
                'GSETTINGS_BACKEND=memory',
                'GTK_CSD=1',
                'G_ENABLE_DIAGNOSTIC=0',
                'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
                'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
                'GSK_RENDERER=' + renderer[0]
              ],
       suite: [ 'gsk', 'gsk-compare', 'gsk-' + renderer[0], 'gsk-compare-' + renderer[0] ])
  endforeach
endforeach

node_parser_tests = [
  'crash1',
  'crash2',
  'crash3',
]

foreach test : node_parser_tests
  test('parser ' + test, node_parser,
       args: [join_paths(meson.current_source_dir(), 'nodeparser', test + '.node')],
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

