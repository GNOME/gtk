# demos/widget-factory

widgetfactory_resources = gnome.compile_resources('widgetfactory_resources',
  'widget-factory.gresource.xml',
  source_dir: '.',
)

executable('gtk4-widget-factory',
  sources: ['widget-factory.c', widgetfactory_resources],
  c_args: common_cflags,
  dependencies: [ libgtk_dep, demo_conf_h ],
  include_directories: confinc,
  gui_app: true,
  link_args: extra_demo_ldflags,
  install: true,
)

# desktop file
install_data('org.gtk.WidgetFactory4.desktop', install_dir: gtk_applicationsdir)

# icons
icontheme_dir = join_paths(gtk_datadir, 'icons/hicolor')

foreach size: ['scalable', 'symbolic']
  install_subdir('data/' + size, install_dir: icontheme_dir)
endforeach

# appdata
install_data('org.gtk.WidgetFactory4.appdata.xml', install_dir: gtk_appdatadir)
