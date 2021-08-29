toml_conf = configuration_data()
toml_conf.set('version', meson.project_version())

gidocgen = find_program('gi-docgen', required: get_option('gtk_doc'))

docs_dir = gtk_datadir / 'doc'

if get_option('gtk_doc') and not build_gir
  error('API reference requires introspection.')
endif

subdir('gdk')
subdir('gsk')
subdir('gtk')
