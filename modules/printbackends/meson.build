print_backends = []

printbackends_subdir = 'gtk-4.0/@0@/printbackends'.format(gtk_binary_version)
printbackends_install_dir = join_paths(get_option('libdir'), printbackends_subdir)
gio_module_dirs += printbackends_install_dir

printbackends_args = [
  '-DGTK_COMPILATION',
  '-DGTK_DISABLE_DEPRECATION_WARNINGS',
  '-DGTK_PRINT_BACKEND_ENABLE_UNSUPPORTED',
  '-D_GLIB_EXTERN=@0@'.format(visibility_define),
] + common_cflags

cups_dep = dependency('cups', version : '>=2.0', required: get_option('print-cups'))
if cups_dep.found()
  print_backends += 'cups'
  shared_module('printbackend-cups',
    sources: [
      'gtkprintbackendcups.c',
      'gtkprintercups.c',
      'gtkcupsutils.c',
      'gtkcupssecretsutils.c',
    ],
    c_args: printbackends_args,
    dependencies: [libgtk_dep, cups_dep, colord_dep],
    name_suffix: module_suffix,
    install_dir: printbackends_install_dir,
    install: true,
  )
else
  # Automatic fall-back to the lpr backend
  print_backends += 'lpr'
  shared_module('printbackend-lpr',
    sources: 'gtkprintbackendlpr.c',
    c_args: printbackends_args,
    dependencies: libgtk_dep,
    name_suffix: module_suffix,
    install_dir: printbackends_install_dir,
    install: true,
  )
endif

rest_dep = dependency('rest-0.7', required : get_option('print-cloudprint'))
json_glib_dep = dependency('json-glib-1.0', required : get_option('print-cloudprint'))
if rest_dep.found() and json_glib_dep.found()
  print_backends += 'cloudprint'
  shared_module('printbackend-cloudprint',
    sources: [
      'gtkprintbackendcloudprint.c',
      'gtkprintercloudprint.c',
      'gtkcloudprintaccount.c',
    ],
    c_args: printbackends_args,
    dependencies: [ libgtk_dep, rest_dep, json_glib_dep ],
    name_suffix: module_suffix,
    install_dir: printbackends_install_dir,
    install: true)
endif

# The 'file' print backend cannot be disabled
print_backends += 'file'
shared_module('printbackend-file',
  sources: 'gtkprintbackendfile.c',
  c_args: printbackends_args,
  dependencies: libgtk_dep,
  name_suffix: module_suffix,
  install_dir: printbackends_install_dir,
  install: true,
)

cdata.set_quoted('GTK_PRINT_BACKENDS', ','.join(print_backends))
