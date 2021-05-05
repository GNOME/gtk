
test('api',
     find_program('api.py', dirs: meson.current_source_dir()),
     suite: ['introspection'],
     env: [
       'GI_TYPELIB_PATH=@0@/gtk:/usr/lib64/girepository-1.0'.format(project_build_root),
       'LD_PRELOAD=@0@/gtk/libgtk-4.so'.format(project_build_root),
     ])
