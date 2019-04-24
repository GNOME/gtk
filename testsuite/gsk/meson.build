testexecdir = join_paths(installed_test_bindir, 'gsk')
testdatadir = join_paths(installed_test_datadir, 'gsk')

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

serialize_deserialize = executable(
  'serialize-deserialize',
  ['serialize-deserialize.c'],
  dependencies: libgtk_dep,
  install: get_option('install-tests'),
  install_dir: testexecdir,
)

# Interesting render nodes proven to be rendered 'correctly' by the GL renderer.
gl_tests = [
  ['cross fade in opacity',        'cross-fade-in-opacity'],
  ['shadow in opacity',        'cross-fade-in-opacity'],
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
    if (renderer[1] == '' or not test.contains(renderer[1]))
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
    endif
  endforeach
endforeach

node_parser_tests = [
  'crash1.invalid',
  'crash2.invalid',
  'crash3.invalid',
  'crash4.invalid',
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

serialize_deserialize_tests = [
  'color.valid',
  'border.valid',
  'shadow.valid',
  'testswitch.valid',
  'widgetfactory.valid',
]

foreach test : serialize_deserialize_tests
  test('serialize-deserialize ' + test, serialize_deserialize,
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
