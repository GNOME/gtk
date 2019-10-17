all_print_backends = [
  'cups',
  'cloudprint',
]

enabled_print_backends = get_option('print-backends').split(',')

if enabled_print_backends.contains('none')
  enabled_print_backends = []
elif enabled_print_backends.contains('all')
  enabled_print_backends = all_print_backends
endif

# The 'file' print backend cannot be disabled
print_backends = ['file']

# Checks to see if we should compile with CUPS backend for GTK
enable_cups = enabled_print_backends.contains('cups')
if enable_cups
  cups_dep = dependency('cups', version : '>=2.0', required: true)
  print_backends += ['cups']
endif

# Checks to see if we should compile with cloudprint backend for GTK
enable_cloudprint = enabled_print_backends.contains('cloudprint')
if enable_cloudprint
  rest_dep = dependency('rest-0.7', required : true)
  json_glib_dep = dependency('json-glib-1.0', required : true)
  if rest_dep.found() and json_glib_dep.found()
    print_backends += ['cloudprint']
  endif
endif

if not cc.has_header('cairo-pdf.h', dependencies : cairo_dep)
  error('Cannot find cairo-pdf.h. You must build Cairo with the pdf backend enabled.')
endif

if os_unix
  if not cc.has_header('cairo-ps.h', dependencies : cairo_dep)
    error('Cannot find cairo-ps.h. You must build Cairo with the postscript backend enabled.')
  endif
  if not cc.has_header('cairo-svg.h', dependencies : cairo_dep)
    error('Cannot find cairo-svg.h. You must build Cairo with the svg backend enabled.')
  endif
endif

# Automatic fall-back to the lpr backend
if not print_backends.contains('cups')
  print_backends += ['lpr']
endif

printbackends_subdir = 'gtk-4.0/@0@/printbackends'.format(gtk_binary_version)
printbackends_install_dir = join_paths(get_option('libdir'), printbackends_subdir)

cdata.set_quoted('GTK_PRINT_BACKENDS', ','.join(print_backends))


enable_colord = get_option('colord')
if enable_colord != 'no'
  want_colord = enable_colord == 'yes'
  colord_dep = dependency('colord', version: '>= 0.1.9', required: want_colord)
  cdata.set('HAVE_COLORD', colord_dep.found())
else
  colord_dep = []
endif

printbackends_args = [
  '-DGTK_COMPILATION',
  '-DGTK_DISABLE_DEPRECATION_WARNINGS',
  '-DGTK_PRINT_BACKEND_ENABLE_UNSUPPORTED',
]

if print_backends.contains('cups')
  shared_module('printbackend-cups',
                'gtkprintbackendcups.c',
                'gtkprintercups.c',
                'gtkcupsutils.c',
                'gtkcupssecretsutils.c',
                c_args: printbackends_args,
                dependencies: [libgtk_dep, cups_dep, colord_dep],
                install_dir: printbackends_install_dir,
                install : true)
endif

if print_backends.contains('cloudprint')
  shared_module('printbackend-cloudprint',
                'gtkprintbackendcloudprint.c',
                'gtkprintercloudprint.c',
                'gtkcloudprintaccount.c',
                c_args: printbackends_args,
                dependencies: [ libgtk_dep, rest_dep, json_glib_dep ],
                install_dir: printbackends_install_dir,
                install : true)
endif

if print_backends.contains('file')
  shared_module('printbackend-file',
                'gtkprintbackendfile.c',
                c_args: printbackends_args,
                dependencies: libgtk_dep,
                install_dir: printbackends_install_dir,
                install : true)
endif

if print_backends.contains('lpr')
  shared_module('printbackend-lpr',
                'gtkprintbackendlpr.c',
                c_args: printbackends_args,
                dependencies: libgtk_dep,
                install_dir: printbackends_install_dir,
                install : true)
endif
