env = environment()
env.prepend('GI_TYPELIB_PATH',
  project_build_root / 'gtk',
)

test('api',
  find_program('api.py', dirs: meson.current_source_dir()),
  suite: ['introspection'],
  depends: [ gtk_gir, libgtk ],
  env: env,
)
