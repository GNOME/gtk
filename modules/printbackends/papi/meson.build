shared_module('printbackend-papi',
  'gtkprinterpapi.c',
  'gtkprintbackendpapi.c',
  c_args : ['-DGTK_PRINT_BACKEND_ENABLE_UNSUPPORTED'],
  dependencies : [libgtk_dep, libpapi],
  install_dir : printbackends_install_dir)
