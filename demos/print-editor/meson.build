executable('gtk4-print-editor',
           ['print-editor.c'],
           c_args: common_cflags,
           dependencies: libgtk_dep,
           include_directories: confinc,
           gui_app: true,
           link_args: extra_demo_ldflags,
           install: true)

# desktop file
install_data('org.gtk.PrintEditor4.desktop', install_dir: gtk_applicationsdir)

# appdata
install_data('org.gtk.PrintEditor4.appdata.xml', install_dir: gtk_appdatadir)
