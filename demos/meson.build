# appdata

appdata_config = configuration_data()
appdata_config.set('BUILD_VERSION', meson.project_version())

gmodule_headers_dep = gmodule_dep.partial_dependency(includes: true)

subdir('gtk-demo')
subdir('node-editor')
subdir('widget-factory')
subdir('print-editor')
if cc.get_id() != 'msvc'
  subdir('icon-editor')
endif
