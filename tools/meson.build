# Installed tools
gtk_tools = [
  ['gtk4-query-settings', ['gtk-query-settings.c'], [libgtk_dep]],
  ['gtk4-builder-tool', ['gtk-builder-tool.c',
                         'gtk-builder-tool-simplify.c',
                         'gtk-builder-tool-validate.c',
                         'gtk-builder-tool-enumerate.c',
                         'gtk-builder-tool-preview.c'], [libgtk_dep] ],
  ['gtk4-update-icon-cache', ['updateiconcache.c'], [ libgtk_static_dep ] ],
  ['gtk4-encode-symbolic-svg', ['encodesymbolic.c'], [ libgtk_static_dep ] ],
]

if os_unix
  gtk_tools += [['gtk4-launch', ['gtk-launch.c'], [libgtk_dep]]]
endif

foreach tool: gtk_tools
  tool_name = tool.get(0)
  tool_srcs = tool.get(1)
  tool_deps = tool.get(2)

  exe = executable(tool_name,
    sources: tool_srcs,
    include_directories: [confinc],
    c_args: common_cflags,
    dependencies: tool_deps,
    install: true,
  )

  set_variable(tool_name.underscorify(), exe) # used in testsuites
endforeach

# Data to install
install_data('gtk4builder.rng', install_dir: gtk_datadir / 'gtk-4.0')

install_data([
    'gtk4builder.loc',
    'gtk4builder.its',
  ],
  install_dir: gtk_datadir / 'gettext/its',
)
