print_backend_cflags += [
  '-DGTK_PRINT_BACKEND_ENABLE_UNSUPPORTED',
]

# Use cpdb backend if present and enabled.
# If not, use cups if present.

if get_option('print-cpdb').enabled() and cpdb_dep.found()
  print_backends += 'cpdb'
  print_backend_sources += files([
    'gtkprintbackendcpdb.c',
    'gtkprintercpdb.c',
    'gtkprintbackendutils.c',
  ])
  print_backend_deps += [cpdb_dep]
  cdata.set('HAVE_PRINTBACKEND_CPDB', 1)
elif cups_dep.found()
  print_backends += 'cups'
  print_backend_sources += files([
    'gtkprintbackendcups.c',
    'gtkprintercups.c',
    'gtkcupsutils.c',
    'gtkcupssecretsutils.c',
    'gtkprintbackendutils.c',
  ])
  print_backend_deps += [cups_dep, colord_dep]
  cdata.set('HAVE_PRINTBACKEND_CUPS', 1)
endif

# The 'file' print backend cannot be disabled
print_backends += 'file'
print_backend_sources += files(['gtkprintbackendfile.c'])

cdata.set_quoted('GTK_PRINT_BACKENDS', ','.join(print_backends))
