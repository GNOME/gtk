test_renderer = executable('test-svg-animation',
  sources: ['test-svg-animation.c', '../../testutils.c'],
  c_args: common_cflags+ ['-DGTK_COMPILATION'],
  dependencies: libgtk_static_dep,
)

test_data = [
  'additive.test',
  'animate-motion.test',
  'clips.test',
  'colors.test',
  'dashes.test',
  'gpa-animations.test',
  'gpa-attachment.test',
  'gpa-attachment2.test',
  'gpa-attachment3.test',
  'gpa-states.test',
  'gpa-transitions.test',
  'gpa-transitions2.test',
  'gpa-transitions3.test',
  'gradient-animation.test',
  'masks.test',
  'negative-delay.test',
  'paths.test',
  'priority.test',
  'states.test',
  'symbolics.test',
  'transform-animation.test',
  'use.test',
]

foreach testname : test_data
  test('svg animation ' + testname, test_renderer,
        args: [ '--tap',
                '-k',
                '--output', meson.current_build_dir(),
                join_paths(meson.current_source_dir(), testname),
              ],
        protocol: 'tap',
        env: [
              'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
              'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir())
             ],
        suite: [ 'svg', 'svg animation' ]
  )
endforeach
