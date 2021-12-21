
app5_resources = gnome.compile_resources(
  'exampleapp5 resources',
  'exampleapp.gresource.xml',
  source_dir: '.'
)

app5_schemas = gnome.compile_schemas(depend_files: files(['org.gtk.exampleapp.gschema.xml']))


app5 = executable(
  'exampleapp5',
  'main.c',
  'exampleapp.c',
  'exampleapp.h',
  'exampleappwin.c',
  'exampleappwin.h',
  app5_resources,
  app5_schemas,
  dependencies: libgtk_dep
)

install_data('org.gtk.exampleapp.gschema.xml', install_dir: gtk_schemasdir)
