demo_conf = configuration_data()
demo_conf.set_quoted('PROFILE', get_option('profile'))
demo_conf.set_quoted('VCS_TAG', '@VCS_TAG@')

demo_conf_h = declare_dependency(
  sources: vcs_tag(
             command: [ 'git', 'rev-parse', '--short', 'HEAD' ],
             fallback: get_option('profile') != 'default' ? 'devel' : '',
             input: configure_file(
                      output: 'demo_conf.h.in',
                      configuration: demo_conf
                    ),
             output: 'demo_conf.h'
           )
)

subdir('constraint-editor')
subdir('gtk-demo')
subdir('icon-browser')
subdir('node-editor')
subdir('widget-factory')
subdir('print-editor')
