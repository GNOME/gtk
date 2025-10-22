test_parser = executable('test-svg-parser',
  sources: ['test-svg-parser.c', '../../testutils.c'],
  c_args: common_cflags,
  dependencies: libgtk_dep,
)

test_data = [
  'all-animations.svg',
  'all-attributes.svg',
  'all-shapes.svg',
  'animate-advanced.svg',
  'animate-basic.svg',
  'animate-begin.svg',
  'animate-chained.svg',
  'animate-colors.svg',
  'animate-motion.svg',
  'animate-paths.svg',
  'animate-timing.svg',
  'animate-transform.svg',
  'bad-attr.svg',
  'clip.svg',
  'containers.svg',
  'dashes.svg',
  'ellipse.svg',
  'filters.svg',
  'gpa-attributes.svg',
  'long-values.svg',
  'many-errors.svg',
  'paint.svg',
  'simple.svg',
  'stroke.svg',
  'times.svg',
  'transforms.svg',
  'use.svg'
]

foreach testname : test_data
  test('svg parser ' + testname, test_parser,
        args: [ '--tap',
                '-k',
                join_paths(meson.current_source_dir(), testname),
              ],
        protocol: 'tap',
        env: [
              'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
              'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir())
             ],
        suite: [ 'svg', 'svg parser' ]
  )
endforeach
