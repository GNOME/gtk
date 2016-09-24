
app2_resources = gnome.compile_resources(
  'exampleapp2 resources',
  'exampleapp.gresource.xml',
  source_dir: '.'
)

app2 = executable(
  'exampleapp2',
  'main.c',
  'exampleapp.c',
  'exampleapp.h',
  'exampleappwin.c',
  'exampleappwin.h',
  app2_resources,
  dependencies: libgtk_dep
)
