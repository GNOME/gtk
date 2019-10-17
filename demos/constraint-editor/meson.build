constraint_editor_sources = [
  'main.c',
  'constraint-editor-application.c',
  'constraint-editor-window.c',
  'constraint-view.c',
  'constraint-editor.c',
  'guide-editor.c',
]

constraint_editor_resources = gnome.compile_resources('constraint_editor_resources',
                                                      'constraint-editor.gresource.xml',
                                                      source_dir: '.')

executable('gtk4-constraint-editor',
           constraint_editor_sources, constraint_editor_resources,
           dependencies: libgtk_dep,
           include_directories: confinc,
           gui_app: true,
           link_args: extra_demo_ldflags,
           install: false)
