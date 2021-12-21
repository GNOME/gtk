
app8_resources = gnome.compile_resources(
  'exampleapp8 resources',
  'exampleapp.gresource.xml',
  source_dir: '.'
)

app8_schemas = gnome.compile_schemas(depend_files: files(['org.gtk.exampleapp.gschema.xml']))


app8 = executable(
  'exampleapp8',
  'main.c',
  'exampleapp.c',
  'exampleapp.h',
  'exampleappwin.c',
  'exampleappwin.h',
  'exampleappprefs.c',
  'exampleappprefs.h',
  app8_resources,
  app8_schemas,
  dependencies: libgtk_dep
)

install_data('org.gtk.exampleapp.gschema.xml', install_dir: gtk_schemasdir)
