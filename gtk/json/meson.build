gtk_json_sources = files([
  'gtkjsonparser.c',
  'gtkjsonprinter.c',
])

gtk_json_deps = [
  libm,
  glib_dep,
  gobject_dep,
  platform_gio_dep,
]

libgtk_json = static_library('gtk_json',
  sources: gtk_json_sources,
  dependencies: gtk_json_deps,
  include_directories: [ confinc, ],
  c_args: [
    '-DGTK_COMPILATION',
    '-DG_LOG_DOMAIN="Gtk"',
  ] + common_cflags,
)

# We don't have link_with: to internal static libs here on purpose, just
# list the dependencies and generated headers and such, for use in the
# "public" libgtk_dep used by internal executables.
libgtk_json_dep = declare_dependency(include_directories: [ confinc, ],
  sources: [ ],
  link_with: [ libgtk_json ],
  dependencies: gtk_json_deps,
)
