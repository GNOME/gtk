
# Note: the list of immodules is in $(srcroot)/meson.build!

foreach l: immodules
  name = l[0]
  sources = l[1]
  cond = l.get(2, true)
  cflags = l.get(3, [])
  immod_sources = []

  # Only get the filename without modules/input
  foreach s: sources
    immod_sources += s.split('/')[2]
  endforeach

  if cond and not (builtin_immodules.contains(name) or builtin_all_immodules)
    immodules_cflags += cflags

    shared_module('im-@0@'.format(name),
                  immod_sources,
                  c_args: common_cflags + immodules_cflags,
                  dependencies: [ libgtk_dep ],
                  install_dir: '@0@/gtk-@1@/@2@/immodules/'.format(gtk_libdir, gtk_api_version, gtk_binary_version),
                  include_directories: [confinc, gtkinc],
                  install : true)
  endif
endforeach