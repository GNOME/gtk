
app7_resources = gnome.compile_resources(
  'exampleapp7 resources',
  'exampleapp.gresource.xml',
  source_dir: '.'
)

app7_schemas = gnome.compile_schemas()


app7 = executable(
  'exampleapp7',
  'main.c',
  'exampleapp.c',
  'exampleapp.h',
  'exampleappwin.c',
  'exampleappwin.h',
  'exampleappprefs.c',
  'exampleappprefs.h',
  app7_resources,
  app7_schemas,
  dependencies: libgtk_dep
)
