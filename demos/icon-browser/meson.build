iconbrowser_sources = [
  'main.c',
  'iconbrowserapp.c',
  'iconbrowserwin.c',
  'iconbrowsericon.c',
  'iconbrowsercontext.c'
]

iconbrowser_resources = gnome.compile_resources('iconbrowser_resources',
                                                'iconbrowser.gresource.xml',
                                                source_dir: '.')

executable('gtk4-icon-browser',
           iconbrowser_sources, iconbrowser_resources,
           c_args: common_cflags,
           dependencies: libgtk_dep,
           include_directories: confinc,
           gui_app: true,
           link_args: extra_demo_ldflags,
           install: true)
