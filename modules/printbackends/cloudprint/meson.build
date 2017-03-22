shared_module('printbackend-cloudprint',
  'gtkprintbackendcloudprint.c',
  'gtkprintercloudprint.c',
  'gtkcloudprintaccount.c',
  c_args : ['-DGTK_PRINT_BACKEND_ENABLE_UNSUPPORTED'],
  dependencies : [libgtk_dep, rest_dep, json_glib_dep],
  install_dir : printbackends_install_dir)
