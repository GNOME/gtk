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

compare_render_tests = [
  'clip-coordinates-3d',
  'clipped_rounded_clip',
  'cross-fade-in-opacity',
  'opacity_clip',
  'outset_shadow_offset_both',
  'outset_shadow_offset_x',
  'outset_shadow_offset_y',
  'outset_shadow_rounded_top',
  'outset_shadow_simple',
  'shadow-in-opacity',
  'blend-normal',
  'blend-difference',
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
  'crash1',
  'crash2',
  'crash3',
  'crash4',
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
  'blend',
  'border',
  'color',
  'debug',
  'rounded-rect',
  'shadow',
  'testswitch',
  'widgetfactory',
]

foreach test : serialize_deserialize_tests
  test('serialize-deserialize ' + test, serialize_deserialize,
       args: [join_paths(meson.current_source_dir(), 'serializedeserialize', test + '.node')],
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
