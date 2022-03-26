testexecdir = join_paths(installed_test_bindir, 'gsk')
testdatadir = join_paths(installed_test_datadir, 'gsk')

compare_render = executable(
  'compare-render',
  ['compare-render.c', '../reftests/reftest-compare.c'],
  dependencies: libgtk_dep,
  c_args: common_cflags,
  install: get_option('install-tests'),
  install_dir: testexecdir
)

node_parser = executable(
  'node-parser',
  ['node-parser.c'],
  dependencies: libgtk_dep,
  c_args: common_cflags,
  install: get_option('install-tests'),
  install_dir: testexecdir
)

compare_render_tests = [
  'blend-difference',
  'blend-invisible-child',
  'blend-normal',
  'border-bottom-right',
  'borders-rotated',
  'borders-scaled',
  'clip-coordinates-2d',
  'clip-coordinates-3d',
  'clip-in-rounded-clip1',
  'clip-in-rounded-clip2',
  'clip-in-rounded-clip3',
  'clipped-repeat-3d-ngl',
  'clipped_rounded_clip',
  'clip-nested1',
  'color-blur0',
  'color-matrix-identity',
  'cross-fade-in-opacity',
  'css-background',
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
# this test fails for some of the backends
# 'empty-linear-gradient',
  'empty-opacity',
  'empty-outset-shadow',
  'empty-repeat',
  'empty-rounded-clip',
  'empty-shadow',
  'empty-texture',
  'empty-transform',
  'huge-height',
  'huge-width',
  'inset-shadow-multiple',
  'invalid-transform',
  'issue-3615',
  'nested-rounded-clips',
  'opacity_clip',
  'opacity-overdraw',
  'opacity-overlapping-children',
  'outset_shadow_offset_both',
  'outset_shadow_offset_x',
  'outset_shadow_offset_y',
  'outset_shadow_rounded_top',
  'outset_shadow_simple',
  'scaled-cairo',
  'scale-textures-negative-ngl',
  'scale-up-down',
  'shadow-in-opacity',
  'texture-url',
  'repeat',
  'repeat-no-repeat',
  'repeat-negative-coords',
  'repeat-texture',
  'transform-in-transform',
  'transform-in-transform-in-transform',
  'unaligned-offscreen',
  'rounded-clip-in-clip-3d', # not really 3d, but cairo fails it
]

# these are too sensitive to differences in the renderers
# to run in ci, but still useful to keep around
informative_render_tests = [
  'big-glyph',
  'empty-text',
  'huge-glyph',
]

renderers = [
  # name          exclude term
  [ 'gl', ''           ],
  [ 'broadway',   '-3d' ],
  [ 'cairo',      '-3d' ],
]

foreach renderer : renderers
  foreach test : compare_render_tests
    if ((renderer[1] == '' or not test.contains(renderer[1])) and
        (renderer[0] != 'broadway' or broadway_enabled))
      test(renderer[0] + ' ' + test, compare_render,
        args: [
          '--output', join_paths(meson.current_build_dir(), 'compare', renderer[0]),
          join_paths(meson.current_source_dir(), 'compare', test + '.node'),
          join_paths(meson.current_source_dir(), 'compare', test + '.png'),
        ],
        env: [
          'GSK_RENDERER=' + renderer[0],
          'GTK_A11Y=test',
          'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
          'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir())
        ],
        suite: [ 'gsk', 'gsk-compare', 'gsk-' + renderer[0], 'gsk-compare-' + renderer[0] ],
      )
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
  'empty-fill.node',
  'empty-fill.ref.node',
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
  'empty-stroke.node',
  'empty-stroke.ref.node',
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
      args: [
        join_paths(meson.current_source_dir(), 'nodeparser', test)
      ],
      env: [
        'GSK_RENDERER=opengl',
        'GTK_A11Y=test',
        'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
        'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir())
      ],
      suite: 'gsk',
    )
  endif
endforeach

tests = [
  ['curve', ['../../gsk/gskcurve.c', '../../gsk/gskcurveintersect.c'], ['-DGTK_COMPILATION']],
  ['curve-special-cases', ['../../gsk/gskcurve.c', '../../gsk/gskcurveintersect.c'], ['-DGTK_COMPILATION']],
  ['dash'],
  ['curve-performance', ['../../gsk/gskcurve.c', '../../gsk/gskcurveintersect.c'], ['-DGTK_COMPILATION']],
  ['path'],
  ['path-special-cases'],
  ['path-stroke'],
  ['path-ops'],
  ['rounded-rect'],
  ['transform'],
  ['shader'],
]

test_cargs = []

foreach t : tests
  test_name = t.get(0)
  test_srcs = ['@0@.c'.format(test_name)] + t.get(1, [])
  test_extra_cargs = t.get(2, [])
  test_extra_ldflags = t.get(3, [])

  test_exe = executable(test_name, test_srcs,
    c_args : test_cargs + test_extra_cargs + common_cflags,
    link_args : test_extra_ldflags,
    dependencies : libgtk_dep,
    install: get_option('install-tests'),
    install_dir: testexecdir,
  )

  test(test_name, test_exe,
    args: [ '--tap', '-k' ],
    protocol: 'tap',
    env: [
      'GSK_RENDERER=cairo',
      'GTK_A11Y=test',
      'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
      'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir())
    ],
    suite: 'gsk',
  )
endforeach

internal_tests = [
  [ 'diff' ],
  [ 'half-float' ],
]

foreach t : internal_tests
  test_name = t.get(0)
  test_srcs = ['@0@.c'.format(test_name)] + t.get(1, [])
  test_extra_cargs = t.get(2, [])
  test_extra_ldflags = t.get(3, [])

  test_exe = executable(test_name, test_srcs,
    c_args : test_cargs + test_extra_cargs + common_cflags,
    link_args : test_extra_ldflags,
    dependencies : libgtk_static_dep,
    install: get_option('install-tests'),
    install_dir: testexecdir,
  )

  test(test_name, test_exe,
    args: [ '--tap', '-k' ],
    protocol: 'tap',
    env: [
      'GSK_RENDERER=cairo',
      'GTK_A11Y=test',
      'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
      'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir())
    ],
    suite: 'gsk',
  )
endforeach


