
app9_resources = gnome.compile_resources(
  'exampleapp9 resources',
  'exampleapp.gresource.xml',
  source_dir: '.'
)

app9_schemas = gnome.compile_schemas(depend_files: files(['org.gtk.exampleapp.gschema.xml']))


app9 = executable(
  'exampleapp9',
  'main.c',
  'exampleapp.c',
  'exampleapp.h',
  'exampleappwin.c',
  'exampleappwin.h',
  'exampleappprefs.c',
  'exampleappprefs.h',
  app9_resources,
  app9_schemas,
  dependencies: libgtk_dep
)

install_data('org.gtk.exampleapp.gschema.xml', install_dir: gtk_schemasdir)
