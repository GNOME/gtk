py = import('python').find_installation('python3', modules: ['gi'])

env = environment()
env.prepend('GI_TYPELIB_PATH',
  project_build_root / 'gtk',
  gi_dep.get_variable(pkgconfig: 'typelibdir'),
)
env.prepend('LD_PRELOAD', project_build_root / 'gtk' / 'libgtk-4.so')

test('api',
  find_program('api.py', dirs: meson.current_source_dir()),
  suite: ['introspection'],
  env: env,
)
