# Print backend config: 'auto' means all backends we have dependencies for,
# the specific backend names mean we should fail if dependencies are missing
all_print_backends = [
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

if enabled_print_backends.contains('cups')
  cups_dep = dependency('cups', version : '>=1.7', required: print_strict_deps)
  if cups_dep.found()
    cups_extra_deps = []

    enable_colord = get_option('colord')
    if enable_colord != 'no'
      want_colord = enable_colord == 'yes'
      colord_dep = dependency('colord', version: '>= 0.1.9', required: want_colord)
      cdata.set('HAVE_COLORD', colord_dep.found() ? 1 : false)
      cups_extra_deps += [colord_dep]
    endif

    print_backends += ['cups']
  else
    message('\'cups\' backend disabled: missing dependencies')
  endif
endif

cdata.set_quoted('GTK_PRINT_BACKENDS', ','.join(print_backends))

disabled_print_backends = []
foreach backend : all_print_backends
  if not print_backends.contains(backend)
    disabled_print_backends += [backend]
  endif
endforeach

# Building

printbackends_args = [
  '-DGTK_COMPILATION',
  '-DGTK_DISABLE_DEPRECATION_WARNINGS',
  '-DGTK_PRINT_BACKEND_ENABLE_UNSUPPORTED',
]
printbackends_subdir = 'gtk-3.0/@0@/printbackends'.format(gtk_binary_version)
printbackends_install_dir = join_paths(get_option('libdir'), printbackends_subdir)

gio_module_dirs += printbackends_install_dir

if print_backends.contains('file')
  shared_module('printbackend-file',
                'file/gtkprintbackendfile.c',
                c_args: printbackends_args,
                dependencies: libgtk_dep,
                install_dir: printbackends_install_dir,
                name_suffix: module_suffix,
                install : true)
endif

if print_backends.contains('lpr')
  shared_module('printbackend-lpr',
                'lpr/gtkprintbackendlpr.c',
                c_args: printbackends_args,
                dependencies: libgtk_dep,
                install_dir: printbackends_install_dir,
                name_suffix: module_suffix,
                install : true)
endif

if print_backends.contains('test')
  shared_module('printbackend-test',
                'test/gtkprintbackendtest.c',
                c_args: printbackends_args,
                dependencies: libgtk_dep,
                install_dir: printbackends_install_dir,
                name_suffix: module_suffix,
                install : true)
endif

if print_backends.contains('cups')
  shared_module('printbackend-cups',
                'cups/gtkprintbackendcups.c',
                'cups/gtkprintercups.c',
                'cups/gtkcupsutils.c',
                'cups/gtkcupssecretsutils.c',
                c_args: printbackends_args,
                dependencies: [libgtk_dep, cups_dep] + cups_extra_deps,
                install_dir: printbackends_install_dir,
                name_suffix: module_suffix,
                install : true)
endif
