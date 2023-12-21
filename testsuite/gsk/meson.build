compare_render = executable('compare-render',
  ['compare-render.c', '../reftests/reftest-compare.c', 'replay-node.c'],
  dependencies: libgtk_dep,
  c_args: common_cflags,
)

skips = [
  # these are too sensitive to differences in the renderers
  # to run in ci, but still useful to keep around
  '--skip', 'big-glyph.node',
  '--skip', 'empty-text.node',
  '--skip', 'huge-glyph.node',
  '--skip', 'rounded-clip-in-clip-3d.node',
  # Both tests fail because of some font rendering issue
  '--skip', 'empty-linear-gradient.node',
  '--skip', 'unaligned-offscreen.node',
  # not really 3d, but cairo fails it
  '--skip', 'upside-down-label-3d',
]

gl_skips = [
  # the GL renderer mishandles transforms with offscreens
  '--skip', 'blend-in-rotate',
  '--skip', 'cross-fade-in-opacity',
  '--skip', 'opacity-overlapping-children',
  '--skip', 'scaled-texture-scale',
  '--skip', 'shadow-opacity',
  '--skip', 'texture-scale-nearest-vs-linear',
  #'--skip', 'blend-normal',
  #'--skip', 'color-matrix-merge',
  #'--skip', 'color-matrix-parsing',
  #'--skip', 'empty-debug',
]

compare_xfails_small_texture = [
  'big-checkerboard',
  'big-checkerboard-scaled-down',
  'big-checkerboard-scaled-down2'
]

test('comparerender-gl', compare_render,
  args: skips + gl_skips + [
    '-k',
    '--verbose',
    '--dir', '@0@/compare'.format(meson.current_source_dir()),
    '--output', '@0@/compare/gl'.format(meson.current_build_dir()),
  ],
  env: [
    'GSK_RENDERER=gl',
    'GTK_A11Y=test',
    'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
    'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir())
  ],
  protocol: 'tap',
  suite: [ 'gsk', 'gsk-compare', 'gsk-gl'],
)

test('comparerender-broadway', compare_render,
  args: skips + [
    '--skip', '-3d',
    '--dir', '@0@/compare'.format(meson.current_source_dir()),
    '--output', '@0@/compare/broadway'.format(meson.current_build_dir()),
  ],
  env: [
    'GSK_RENDERER=broadway',
    'GTK_A11Y=test',
    'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
    'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir())
  ],
  protocol: 'tap',
  suite: [ 'gsk', 'gsk-compare', 'gsk-broadway'],
)

test('comparerender-cairo', compare_render,
  args: skips + [
    '--skip', '-3d',
    '--dir', '@0@/compare'.format(meson.current_source_dir()),
    '--output', '@0@/compare/cairo'.format(meson.current_build_dir()),
  ],
  env: [
    'GSK_RENDERER=cairo',
    'GTK_A11Y=test',
    'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
    'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir())
  ],
  protocol: 'tap',
  suite: [ 'gsk', 'gsk-compare', 'gsk-cairo'],
)

node_parser = executable('node-parser', 'node-parser.c',
  dependencies: libgtk_dep,
  c_args: common_cflags,
)

test('nodeparser', node_parser,
  env: [
    'GSK_RENDERER=opengl',
    'GTK_A11Y=test',
    'G_TEST_SRCDIR=@0@/nodeparser'.format(meson.current_source_dir()),
    'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir())
  ],
  protocol: 'tap',
  suite: 'gsk',
)

# offload does not work outside of linux
if os_linux
  offload = executable('offload', 'offload.c', 'gskrendernodeattach.c',
    dependencies : libgtk_static_dep,
    c_args: common_cflags,
  )

  test('offload', offload,
    env: [
      'GSK_RENDERER=opengl',
      'GTK_A11Y=test',
      'G_TEST_SRCDIR=@0@/offload'.format(meson.current_source_dir()),
      'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir())
    ],
    protocol: 'tap',
    suite: 'gsk',
  )
endif

tests = [
  ['transform'],
  ['shader'],
  ['path', [ 'path-utils.c' ] ],
  ['path-special-cases'],
]

test_cargs = []

foreach t : tests
  test_name = t.get(0)
  test_srcs = ['@0@.c'.format(test_name)] + t.get(1, [])
  test_extra_cargs = t.get(2, [])
  test_extra_ldflags = t.get(3, [])

  test_exe = executable(test_name, test_srcs,
    dependencies : libgtk_dep,
    c_args : test_cargs + test_extra_cargs + common_cflags,
    link_args : test_extra_ldflags,
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
  [ 'curve' ],
  [ 'curve-special-cases' ],
  [ 'path-private' ],
  [ 'diff' ],
  [ 'half-float' ],
  ['rounded-rect'],
  ['misc'],
  ['boundingbox'],
]

foreach t : internal_tests
  test_name = t.get(0)
  test_srcs = ['@0@.c'.format(test_name)] + t.get(1, [])
  test_extra_cargs = t.get(2, [])
  test_extra_ldflags = t.get(3, [])

  test_exe = executable(test_name,
    sources: test_srcs,
    dependencies : libgtk_static_dep,
    c_args : test_cargs + test_extra_cargs + common_cflags + ['-DGTK_COMPILATION'],
    link_args : test_extra_ldflags,
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


