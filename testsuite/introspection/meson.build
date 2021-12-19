py = import('python').find_installation('python3', modules: ['gi'])

test('api',
     find_program('api.py', dirs: meson.current_source_dir()),
     suite: ['introspection'],
     env: [
       'GI_TYPELIB_PATH=@0@/gtk:@1@'.format(project_build_root,
          gi_dep.get_pkgconfig_variable('typelibdir')),
       'LD_PRELOAD=@0@/gtk/libgtk-4.so'.format(project_build_root),
     ])
