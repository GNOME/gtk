
app4_resources = gnome.compile_resources(
  'exampleapp4 resources',
  'exampleapp.gresource.xml',
  source_dir: '.'
)

app4 = executable(
  'exampleapp4',
  'main.c',
  'exampleapp.c',
  'exampleapp.h',
  'exampleappwin.c',
  'exampleappwin.h',
  app4_resources,
  dependencies: libgtk_dep
)
