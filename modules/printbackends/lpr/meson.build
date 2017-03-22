shared_module('printbackend-lpr',
  'gtkprintbackendlpr.c',
  c_args : ['-DGTK_COMPILATION', '-DGTK_PRINT_BACKEND_ENABLE_UNSUPPORTED'],
  dependencies : libgtk_dep,
  install_dir : printbackends_install_dir)
