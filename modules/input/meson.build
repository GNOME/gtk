
# Note: the list of immodules is in $(srcroot)/meson.build!

immodules_install_dir = '@0@/gtk-@1@/@2@/immodules/'.format(gtk_libdir, gtk_api_version, gtk_binary_version)
gio_module_dirs += immodules_install_dir

foreach l: immodules
  name = l[0]
  immod_sources = l[1]
  cond = l.get(2, true)
  cflags = l.get(3, [])

  if cond and not (builtin_immodules.contains(name) or builtin_all_immodules)
    shared_module('im-@0@'.format(name),
                  immod_sources,
                  c_args: common_cflags + cflags,
                  dependencies: [ libgtk_dep ],
                  install_dir: immodules_install_dir,
                  include_directories: [confinc, gtkinc],
                  install : true,
                  name_suffix: module_suffix,
                  name_prefix: '')
  endif
endforeach
