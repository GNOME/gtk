# appdata

appdata_config = configuration_data()
appdata_config.set('BUILD_VERSION', meson.project_version())

gmodule_headers_dep = gmodule_dep.partial_dependency(includes: true)

demo_kwargs = {}
if meson.version().version_compare('>= 1.8.0')
  demo_kwargs += { 'android_exe_type': 'application' }
endif

subdir('gtk-demo')
subdir('node-editor')
subdir('widget-factory')
subdir('print-editor')
if cc.get_id() != 'msvc'
  subdir('icon-editor')
endif
