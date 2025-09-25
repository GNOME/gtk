gtk_a11y_src = []
gtk_a11y_backends = []
gtk_a11y_backend_info = {}

if os_unix
  gtk_a11y_backends += 'atspi'
endif

if gtk_a11y_backends.contains('atspi')
  gtk_a11y_backend_info += { 'atspi': { 'description': 'GTK AT-SPI accessibility support' } }

  subdir('atspi')

  gtk_a11y_src += files([
    'gtkatspiaction.c',
    'gtkatspicache.c',
    'gtkatspicomponent.c',
    'gtkatspicontext.c',
    'gtkatspieditabletext.c',
    'gtkatspipango.c',
    'gtkatspiroot.c',
    'gtkatspiselection.c',
    'gtkatspisocket.c',
    'gtkatspitextbuffer.c',
    'gtkatspitext.c',
    'gtkatspiutils.c',
    'gtkatspivalue.c',
    'gtkatspihypertext.c',
    'gtkatspihyperlink.c',
  ])

  gtk_atspi_public_headers = files([
    'gtkatspisocket.h',
  ])

  install_headers(gtk_atspi_public_headers, 'gtkatspi.h', subdir: 'gtk-4.0/gtk/a11y/')
endif

if accesskit_dep.found()
  gtk_a11y_backends += 'accesskit'
endif

if gtk_a11y_backends.contains('accesskit')
  gtk_a11y_backend_info += { 'accesskit': { 'description': 'GTK AccessKit accessibility support' } }

  gtk_a11y_src += files([
    'gtkaccesskitcontext.c',
    'gtkaccesskitroot.c',
  ])
endif
