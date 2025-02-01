gdk_android_glue = static_library('glue', 'glue.c',
  c_args: [ common_cflags ],
  dependencies: [
    glib_dep,
    libgtk_dep,
    cc.find_library('log')
  ]
)

gdk_android_glue_dep = declare_dependency(link_whole: gdk_android_glue)
meson.override_dependency('gtk4-android-glue', gdk_android_glue_dep)
