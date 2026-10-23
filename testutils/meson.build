testutils_inc = include_directories('.')
testutils_deps = [
  glib_dep,
  gio_dep,
  graphene_dep,
  cairo_dep,
  pango_dep,
  pixbuf_dep,
  libgsk_dep,
]

testutils_lib = static_library('testutils', 'testutils.c', 'diff/diffreg.c',
  c_args: common_cflags + ['-DGDK_STATIC_COMPILATION'],
  include_directories: testutils_inc,
  dependencies: testutils_deps,
  install: false
)

testutils_dep = declare_dependency(
  sources: ['testutils.h'],
  include_directories: testutils_inc,
  link_with: testutils_lib,
  dependencies: testutils_deps,
)
