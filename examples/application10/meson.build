app10_resources = gnome.compile_resources('exampleapp10 resources',
  'exampleapp.gresource.xml',
  source_dir: '.')

app10_schemas = gnome.compile_schemas()

executable('exampleapp10',
  'main.c',
  'exampleapp.c',
  'exampleappwin.c',
  'exampleappprefs.c',
  app10_resources,
  app10_schemas,
  dependencies: libgtk_dep)
