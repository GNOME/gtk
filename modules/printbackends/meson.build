# Print backend config: 'auto' means all backends we have dependencies for,
# the specific backend names mean we should fail if dependencies are missing
all_print_backends = [
  'cloudprint',
  'cups',
  'file',
  'lpr',
  'papi',
  'test',
]

auto_print_backends = []
foreach backend: all_print_backends
  if backend != 'test' and os_unix
    auto_print_backends += backend
  endif
endforeach

print_strict_deps = true
if get_option('print_backends') == 'auto'
  enabled_print_backends = auto_print_backends
  print_strict_deps = false
else
  wanted_print_backends = get_option('print_backends').split(',')
  enabled_print_backends = []
  foreach backend: wanted_print_backends
    if backend != ''
      if not all_print_backends.contains(backend)
        error('print backend \'@0@\' unknown'.format(backend))
      endif
      enabled_print_backends += backend
    endif
  endforeach
endif

print_backends = []

if not enabled_print_backends.contains('file')
  if os_unix
    error('\'file\' print backed needs to be enabled')
  endif
else
  print_backends += ['file']
endif

if enabled_print_backends.contains('lpr')
  print_backends += ['lpr']
endif

if enabled_print_backends.contains('test')
  print_backends += ['test']
endif

if enabled_print_backends.contains('papi')
  # TODO
  if print_strict_deps
    error('\'papi\' backend not supported with meson yet')
  endif
endif

if enabled_print_backends.contains('cloudprint')
  rest_dep = dependency('rest-0.7', required : print_strict_deps)
  json_glib_dep = dependency('json-glib-1.0', required : print_strict_deps)
  if rest_dep.found() and json_glib_dep.found()
    print_backends += ['cloudprint']
  else
    message('\'cloudprint\' backend disabled: missing dependencies')
  endif
endif

if enabled_print_backends.contains('cups')
  cups_error = ''
  if cc.has_header('cups/cups.h')
    cups_major_version = cc.compute_int('CUPS_VERSION_MAJOR', prefix : '#include <cups/cups.h>')
    cups_minor_version = cc.compute_int('CUPS_VERSION_MINOR', prefix : '#include <cups/cups.h>')
    cups_version = '@0@.@1@'.format(cups_major_version, cups_minor_version)
    message('Found CUPS version: @0@'.format(cups_version))
    if cups_version.version_compare('<1.7')
      cups_error = 'Need CUPS version >= 1.7'
    else
      libcups = cc.find_library('cups', required : true)
    endif
  else
    cups_error = 'Cannot find CUPS headers in default prefix.'
  endif

  enable_colord = get_option('colord')
  if enable_colord != 'no'
    want_colord = enable_colord == 'yes'
    colord_dep = dependency('colord', version: '>= 0.1.9', required: want_colord)
    cdata.set('HAVE_COLORD', colord_dep.found() ? 1 : false)
  else
    cups_colord_dep = []
  endif

  if cups_error != ''
    if print_strict_deps
      error(cups_error)
    else
      message(cups_error)
    endif
  else
    print_backends += ['cups']
  endif
endif

cdata.set_quoted('GTK_PRINT_BACKENDS', ','.join(print_backends))

# Building

printbackends_args = [
  '-DGTK_COMPILATION',
  '-DGTK_DISABLE_DEPRECATION_WARNINGS',
  '-DGTK_PRINT_BACKEND_ENABLE_UNSUPPORTED',
]
printbackends_subdir = 'gtk-3.0/@0@/printbackends'.format(gtk_binary_version)
printbackends_install_dir = join_paths(get_option('libdir'), printbackends_subdir)

if print_backends.contains('file')
  shared_module('printbackend-file',
                'file/gtkprintbackendfile.c',
                c_args: printbackends_args,
                dependencies: libgtk_dep,
                install_dir: printbackends_install_dir,
                install : true)
endif

if print_backends.contains('lpr')
  shared_module('printbackend-lpr',
                'lpr/gtkprintbackendlpr.c',
                c_args: printbackends_args,
                dependencies: libgtk_dep,
                install_dir: printbackends_install_dir,
                install : true)
endif

if print_backends.contains('test')
  shared_module('printbackend-test',
                'test/gtkprintbackendtest.c',
                c_args: printbackends_args,
                dependencies: libgtk_dep,
                install_dir: printbackends_install_dir,
                install : true)
endif

if print_backends.contains('cloudprint')
    shared_module('printbackend-cloudprint',
                  'cloudprint/gtkprintbackendcloudprint.c',
                  'cloudprint/gtkprintercloudprint.c',
                  'cloudprint/gtkcloudprintaccount.c',
                  c_args: printbackends_args,
                  dependencies: [ libgtk_dep, rest_dep, json_glib_dep ],
                  install_dir: printbackends_install_dir,
                  install : true)
endif

if print_backends.contains('cups')
  shared_module('printbackend-cups',
                'cups/gtkprintbackendcups.c',
                'cups/gtkprintercups.c',
                'cups/gtkcupsutils.c',
                'cups/gtkcupssecretsutils.c',
                c_args: printbackends_args,
                dependencies: [libgtk_dep, libcups, colord_dep],
                install_dir: printbackends_install_dir,
                install : true)
endif
