if get_option('gtk_doc')
  doc_shooter_resources = gnome.compile_resources('shooter_resources',
                                                  'shooter.gresource.xml',
                                                  source_dir: '.')

  doc_shooter_sources = [
    'shooter.c',
    'shadow.c',
    'widgets.c',
    '../../tests/gtkgears.c',
  ]

  doc_shooter = executable('doc-shooter',
                           doc_shooter_sources, doc_shooter_resources,
                           include_directories: [ confinc, gdkinc, gtkinc, testinc, ],
                           dependencies: libgtk_dep)
endif
