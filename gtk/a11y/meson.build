gtk_a11y_src = []
gtk_a11y_backends = []

if os_unix
  gtk_a11y_backends += 'atspi'
endif

if gtk_a11y_backends.contains('atspi')
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
    'gtkatspitextbuffer.c',
    'gtkatspitext.c',
    'gtkatspiutils.c',
    'gtkatspivalue.c',
  ])
endif
