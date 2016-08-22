
widgetfactory_sources = [
  'widget-factory.c'
]

widgetfactory_resources = gnome.compile_resources(
  'widgetfactory_resources',
  'widget-factory.gresource.xml',
  source_dir: '.'
)

widget_factory = executable(
  'gtk3-widget-factory',
  widgetfactory_sources,
  widgetfactory_resources,
  dependencies: libgtk_dep,
  include_directories : confinc,
  gui_app: true
)
