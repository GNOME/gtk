
app10_resources = gnome.compile_resources(
  'exampleapp10 resources',
  'exampleapp.gresource.xml',
  source_dir: '.'
)

app10_schemas = gnome.compile_schemas()


app10 = executable(
  'exampleapp10',
  'main.c',
  'exampleapp.c',
  'exampleapp.h',
  'exampleappwin.c',
  'exampleappwin.h',
  'exampleappprefs.c',
  'exampleappprefs.h',
  app10_resources,
  app10_schemas,
  dependencies: libgtk_dep
)
