shared_module('printbackend-cups',
  'gtkprintbackendcups.c',
  'gtkprintercups.c',
  'gtkcupsutils.c',
  'gtkcupssecretsutils.c',
  c_args : ['-DGTK_COMPILATION', '-DGTK_PRINT_BACKEND_ENABLE_UNSUPPORTED'],
  dependencies : [libgtk_dep, libcups, colord_dep],
  install_dir : printbackends_install_dir)
