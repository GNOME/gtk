toml_conf = configuration_data()
toml_conf.set('version', meson.project_version())

gidocgen = find_program('gi-docgen', required: get_option('gtk_doc'))

docs_dir = gtk_datadir / 'doc'

subdir('gdk')
subdir('gsk')
subdir('gtk')
