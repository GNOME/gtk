test_renderer = executable('test-svg-renderer',
  sources: ['test-svg-renderer.c', '../../testutils.c'],
  c_args: common_cflags,
  dependencies: libgtk_dep,
)

test_data = [
  'circle.svg',
  'clip.svg',
  'ellipse.svg',
  'filter.svg',
  'initial-state1.svg',
  'initial-state2.svg',
  'initial-state3.svg',
  'initial-state4.svg',
  'mask.svg',
  'more-filters.svg',
  'not-rendered.svg',
  'opacity.svg',
  'rect.svg',
  'rect2.svg',
  'rounded-rect.svg',
  'style.svg',
  'use.svg',
]

foreach testname : test_data
  test('svg renderer ' + testname, test_renderer,
        args: [ '--tap',
                '-k',
                join_paths(meson.current_source_dir(), testname),
              ],
        protocol: 'tap',
        env: [
              'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
              'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir())
             ],
        suite: [ 'svg', 'svg renderer']
  )
endforeach
