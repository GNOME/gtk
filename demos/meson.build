gen_demo_header = find_program('../build-aux/meson/gen-demo-header.py')
demo_profile = get_option('profile')

demo_conf_h = declare_dependency(
  sources: custom_target('demo-header',
    command: [gen_demo_header, meson.source_root(), demo_profile],
    capture: true,
    output: 'demo_conf.h',
    build_by_default: true,
    build_always_stale: true,
  )
)

# appdata
today = 'unknown'
date = find_program('date',
         required: false)
if date.found()
  r = run_command(date, '-I')
  if r.returncode() == 0
    today = r.stdout().strip()
  endif
endif

appdata_config = configuration_data()
appdata_config.set('BUILD_VERSION', meson.project_version())
appdata_config.set('BUILD_DATE', today)

subdir('constraint-editor')
subdir('gtk-demo')
subdir('icon-browser')
subdir('node-editor')
subdir('widget-factory')
subdir('print-editor')
