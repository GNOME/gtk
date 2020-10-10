gtk_a11y_src = []
gtk_a11y_backends = []

if os_unix
  gtk_a11y_backends += 'atspi'
endif

if gtk_a11y_backends.contains('atspi')
  subdir('atspi')

  gtk_a11y_src += files([
    'gtkatspicache.c',
    'gtkatspicontext.c',
    'gtkatspiroot.c',
    'gtkatspiutils.c',
    'gtkatspipango.c',
  ])
endif
