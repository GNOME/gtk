
app6_resources = gnome.compile_resources(
  'exampleapp6 resources',
  'exampleapp.gresource.xml',
  source_dir: '.'
)

app6_schemas = gnome.compile_schemas(depend_files: files(['org.gtk.exampleapp.gschema.xml']))


app6 = executable(
  'exampleapp6',
  'main.c',
  'exampleapp.c',
  'exampleapp.h',
  'exampleappwin.c',
  'exampleappwin.h',
  'exampleappprefs.c',
  'exampleappprefs.h',
  app6_resources,
  app6_schemas,
  dependencies: libgtk_dep
)

install_data('org.gtk.exampleapp.gschema.xml', install_dir: gtk_schemasdir)
