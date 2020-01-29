# Installed tools
gtk_tools = [
  ['gtk4-query-settings', ['gtk-query-settings.c']],
  ['gtk4-builder-tool', ['gtk-builder-tool.c',
                         'gtk-builder-tool-simplify.c',
                         'gtk-builder-tool-validate.c',
                         'gtk-builder-tool-enumerate.c',
                         'gtk-builder-tool-precompile.c',
                         'gtk-builder-tool-preview.c']],
  ['gtk4-update-icon-cache', ['updateiconcache.c', 'gtkiconcachevalidator.c']],
  ['gtk4-encode-symbolic-svg', ['encodesymbolic.c', 'gdkpixbufutils.c']],
]

if os_unix
  gtk_tools += [['gtk4-launch', ['gtk-launch.c']]]
endif

foreach tool: gtk_tools
  tool_name = tool.get(0)
  tool_srcs = tool.get(1)

  exe = executable(tool_name, tool_srcs,
                   include_directories: [confinc],
                   c_args: gtk_cargs,
                   dependencies: libgtk_dep,
                   install: true)

  set_variable(tool_name.underscorify(), exe) # used in testsuites
endforeach

# Data to install
install_data('gtk4builder.rng',
             install_dir: join_paths(gtk_datadir, 'gtk-4.0'))

install_data('gtk4builder.loc', 'gtk4builder.its',
             install_dir: join_paths(gtk_datadir, 'gettext/its'))

