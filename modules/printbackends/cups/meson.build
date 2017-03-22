enable_colord = get_option('enable-colord')
if enable_colord != 'no'
  want_colord = enable_colord == 'yes'
  colord_dep = dependency('colord', version: '>= 0.1.9', required: want_colord)
  cdata.set('HAVE_COLORD', colord_dep.found())
else
  colord_dep = []
endif

shared_module('printbackend-cups',
  'gtkprintbackendcups.c',
  'gtkprintercups.c',
  'gtkcupsutils.c',
  'gtkcupssecretsutils.c',
  c_args : ['-DGTK_COMPILATION', '-DGTK_PRINT_BACKEND_ENABLE_UNSUPPORTED'],
  dependencies : [libgtk_dep, libcups, colord_dep],
  install_dir : printbackends_install_dir)
