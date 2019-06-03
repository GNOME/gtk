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

compare_render_tests = [
  'big-glyph',
  'blend-normal',
  'blend-difference',
  'clip-coordinates-3d',
  'clipped_rounded_clip',
  'color-blur0',
  'color-matrix-identity',
  'cross-fade-in-opacity',
  'empty-blend',
  'empty-blur',
  'empty-border',
  'empty-cairo',
  'empty-clip',
  'empty-color',
  'empty-color-matrix',
  'empty-container',
  'empty-cross-fade',
  'empty-debug',
  'empty-inset-shadow',
  'empty-linear-gradient',
  'empty-opacity',
  'empty-outset-shadow',
  'empty-repeat',
  'empty-rounded-clip',
  'empty-shadow',
  'empty-text',
  'empty-texture',
  'empty-transform',
  'huge-glyph',
  'opacity_clip',
  'outset_shadow_offset_both',
  'outset_shadow_offset_x',
  'outset_shadow_offset_y',
  'outset_shadow_rounded_top',
  'outset_shadow_simple',
  'scaled-cairo',
  'shadow-in-opacity',
  'texture-url',
  'color-matrix-identity',
  'clip-nested1',
  'scale-up-down'
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
           args: ['--output', join_paths(meson.current_build_dir(), 'compare', renderer[0]),
                  join_paths(meson.current_source_dir(), 'compare', test + '.node'),
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
  'blend.node',
  'border.node',
  'color.node',
  'crash1.errors',
  'crash1.node',
  'crash1.ref.node',
  'crash2.errors',
  'crash2.node',
  'crash2.ref.node',
  'crash3.node',
  'crash3.ref.node',
  'crash4.errors',
  'crash4.node',
  'crash4.ref.node',
  'debug.node',
  'empty-blend.node',
  'empty-blend.ref.node',
  'empty-blur.node',
  'empty-blur.ref.node',
  'empty-border.node',
  'empty-border.ref.node',
  'empty-cairo.node',
  'empty-cairo.ref.node',
  'empty-clip.node',
  'empty-clip.ref.node',
  'empty-color.node',
  'empty-color.ref.node',
  'empty-color-matrix.node',
  'empty-color-matrix.ref.node',
  'empty-container.node',
  'empty-container.ref.node',
  'empty-cross-fade.node',
  'empty-cross-fade.ref.node',
  'empty-debug.node',
  'empty-debug.ref.node',
  'empty-inset-shadow.node',
  'empty-inset-shadow.ref.node',
  'empty-linear-gradient.node',
  'empty-linear-gradient.ref.node',
  'empty-opacity.node',
  'empty-opacity.ref.node',
  'empty-outset-shadow.node',
  'empty-outset-shadow.ref.node',
  'empty-repeat.node',
  'empty-repeat.ref.node',
  'empty-rounded-clip.node',
  'empty-rounded-clip.ref.node',
  'empty-shadow.node',
  'empty-shadow.ref.node',
  'empty-text.node',
  'empty-text.ref.node',
  'empty-texture.node',
  'empty-texture.ref.node',
  'empty-transform.node',
  'empty-transform.ref.node',
  'rounded-rect.node',
  'shadow.node',
  'testswitch.node',
  'widgetfactory.node',
]

foreach test : node_parser_tests
  if test.endswith('.node') and not test.endswith('.ref.node')
    test('parser ' + test, node_parser,
         args: [ join_paths(meson.current_source_dir(), 'nodeparser', test)
               ],
         env: [ 'GIO_USE_VOLUME_MONITOR=unix',
                'GSETTINGS_BACKEND=memory',
                'GTK_CSD=1',
                'G_ENABLE_DIAGNOSTIC=0',
                'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
                'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
                'GSK_RENDERER=opengl'
              ],
         suite: 'gsk')
  endif
endforeach
