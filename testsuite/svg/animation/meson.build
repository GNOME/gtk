test_renderer = executable('test-svg-animation',
  sources: ['test-svg-animation.c', '../../testutils.c'],
  c_args: common_cflags,
  dependencies: libgtk_static_dep,
)

test_data = [
  'additive.svg.0',
  'additive.svg.1000',
  'additive.svg.2000',
  'additive.svg.3000',
  'additive.svg.4000',
  'additive.svg.5000',
  'additive.svg.6000',
  'animate-motion.svg.0',
  'animate-motion.svg.5000',
  'animate-motion.svg.10000',
  'animate-motion.svg.20000',
  'dashes.svg.0',
  'dashes.svg.5000',
  'inherit.svg.0',
  'inherit.svg.1000',
  'inherit.svg.2000',
  'priority.svg.0',
  'priority.svg.1000',
  'priority.svg.2000',
  'priority.svg.3000',
  'priority.svg.4000',
  'priority.svg.5000',
  'priority.svg.6000',
  'states.svg.0',
  'states.svg.500',
  'states.svg.1000',
  'states.svg.5000',
  'states.svg.10000',
  'states.svg.10500',
  'transform-animation.svg.0',
  'transform-animation.svg.1000',
  'transform-animation.svg.1500',
  'transform-animation.svg.1750',
]

foreach testname : test_data
  test('svg animation ' + testname, test_renderer,
        args: [ '--tap',
                '-k',
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
