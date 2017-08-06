# demos/widget-factory

widgetfactory_resources = gnome.compile_resources('widgetfactory_resources',
                                                  'widget-factory.gresource.xml',
                                                  source_dir: '.')

executable('gtk4-widget-factory',
           'widget-factory.c', widgetfactory_resources,
           dependencies: libgtk_dep,
           include_directories: confinc,
           gui_app: true,
           install: true)

# desktop file
install_data('gtk4-widget-factory.desktop', install_dir: gtk_applicationsdir)

# icons
icontheme_dir = join_paths(gtk_datadir, 'icons/hicolor')

foreach icon_size : ['16x16', '22x22', '24x24', '32x32', '48x48', '256x256', ]
  icons_dir = join_paths(icontheme_dir, '@0@/apps'.format(icon_size))
  install_data('data/@0@/gtk4-widget-factory.png'.format(icon_size),
               install_dir: icons_dir)
  install_data('data/@0@/gtk4-widget-factory-symbolic.symbolic.png'.format(icon_size),
               install_dir: icons_dir)
endforeach
