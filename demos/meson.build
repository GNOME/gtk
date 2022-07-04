gen_demo_header = find_program('../build-aux/meson/gen-demo-header.py')
demo_profile = get_option('profile')

demo_conf_h = declare_dependency(
  sources: custom_target('demo-header',
    command: [gen_demo_header, meson.project_source_root(), demo_profile],
    capture: true,
    output: 'demo_conf.h',
    build_by_default: true,
    build_always_stale: true,
  )
)

# appdata

appdata_config = configuration_data()
appdata_config.set('BUILD_VERSION', meson.project_version())

subdir('constraint-editor')
subdir('gtk-demo')
subdir('icon-browser')
subdir('node-editor')
subdir('widget-factory')
subdir('print-editor')
subdir('font-explorer')
