print_backends = ['file']

# Checks to see if we should compile with CUPS backend for GTK+
enable_cups = get_option('enable-cups-print-backend')
if enable_cups != 'no'
  want_cups = enable_cups == 'yes'
  #cups_config = find_program('cups-config', required : want_cups)
  #if cups_config.found()
  # FIXME: eek, see configure.ac (we're just not going to support non-standar prefix for now)
  #endif
  if cc.has_header('cups/cups.h')
    # No cc.compute_int() yet: https://github.com/mesonbuild/meson/issues/435
    cups_major_version = 0
    cups_minor_version = -1
    foreach check_ver : [1,2,3]
      if cups_major_version == 0
        if cc.compiles('''#include <cups/cups.h>
                          #if CUPS_VERSION_MAJOR != @0@
                          #error "Not this version"
                          #endif'''.format(check_ver),
                          name : 'Check CUPS major version @0@'.format(check_ver))
          cups_major_version = check_ver
        endif
      endif
    endforeach
    foreach check_ver : [0,1,2,3,4,5,6,7,8,9]
      if cups_major_version > 0 and cups_minor_version == -1
        if cc.compiles('''#include <cups/cups.h>
                          #if CUPS_VERSION_MINOR != @0@
                          #error "Not this version"
                          #endif'''.format(check_ver),
                          name : 'Check CUPS minor version @0@'.format(check_ver))
          cups_minor_version = check_ver
        endif
      endif
    endforeach
    if cups_major_version > 0 and cups_minor_version >= 0
      message('Found CUPS version: @0@.@1@'.format(cups_major_version, cups_minor_version))
      if cups_major_version > 1 or cups_minor_version >= 2
        if cups_major_version > 1 or cups_minor_version >= 6
          cdata.set('HAVE_CUPS_API_1_6', 1)
        endif

        if cc.compiles('#include <cups/http.h> \n http_t http; char *s = http.authstring;')
          cdata.set('HAVE_HTTP_AUTHSTRING', 1,
            description :'Define if cups http_t authstring field is accessible')
        endif
        libcups = cc.find_library('cups', required : want_cups)
        if libcups.found() and cc.has_function('httpGetAuthString', dependencies : libcups)
          cdata.set('HAVE_HTTPGETAUTHSTRING', 1)
        endif

        print_backends += ['cups']
      elif want_cups
        error('Need CUPS version >= 1.2')
      endif
    else
      error('Could not determine CUPS version from header files.')
    endif
  elif want_cups
    error('Cannot find CUPS headers in default prefix.')
  endif
endif

# Checks to see if we should compile with PAPI backend for GTK+
enable_papi = get_option('enable-papi-print-backend')
if enable_papi != 'no'
  want_papi = enable_papi == 'yes'
  libpapi = cc.find_library('libpapi', required : false)
  if libpapi.found() and cc.has_function('papiServiceCreate', args : '-lpapi')
    print_backends += ['papi']
  elif want_papi
    error('Cannot find libpapi.')
  endif
endif

# Checks to see if we should compile with cloudprint backend for GTK+
enable_cloudprint = get_option('enable-cloudprint-print-backend')
if enable_cloudprint != 'no'
  want_cloudprint = enable_cloudprint == 'yes'
  rest_dep = dependency('rest-0.7', required : want_cloudprint)
  json_glib_dep = dependency('json-glib-1.0', required : want_cloudprint)
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

if get_option('enable-test-print-backend') == 'yes'
  print_backends += ['test']
endif

if not print_backends.contains('papi') and not print_backends.contains('cups')
  print_backends += ['lpr']
endif

printbackends_subdir = 'gtk-4.0/@0@/printbackends'.format(gtk_binary_version)
printbackends_install_dir = join_paths(get_option('libdir'), printbackends_subdir)

message('Print backends: ' + ' '.join(print_backends))

foreach print_backend : print_backends
  subdir(print_backend)
endforeach
