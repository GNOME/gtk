if x11_enabled and get_option('gtk_doc')
  doc_shooter_sources = [
    'shadow.c',
    'shooter.c',
    'widgets.c',
    '../../tests/gtkgears.c',
  ]

  doc_shooter = executable('doc-shooter', doc_shooter_sources,
                           include_directories: [ confinc, gdkinc, gtkinc, testinc, ],
                           dependencies: libgtk_dep)
endif
