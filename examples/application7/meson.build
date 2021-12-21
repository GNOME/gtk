
app7_resources = gnome.compile_resources(
  'exampleapp7 resources',
  'exampleapp.gresource.xml',
  source_dir: '.'
)

app7_schemas = gnome.compile_schemas(depend_files: files(['org.gtk.exampleapp.gschema.xml']))


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

install_data('org.gtk.exampleapp.gschema.xml', install_dir: gtk_schemasdir)
