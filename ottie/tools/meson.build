# Installed tools
ottie_tools = [
  ['ottie', ['ottie.c']],
]

foreach tool: ottie_tools
  tool_name = tool.get(0)
  tool_srcs = tool.get(1)

  exe = executable(tool_name,
    sources: tool_srcs,
    include_directories: [confinc],
    c_args: common_cflags,
    dependencies: [libgtk_dep],
    link_with: libottie,
    install: true,
  )

  set_variable(tool_name.underscorify(), exe) # used in testsuites
endforeach
