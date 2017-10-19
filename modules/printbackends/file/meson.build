shared_module('printbackend-file',
              'gtkprintbackendfile.c',
              c_args: [
                '-DGTK_COMPILATION',
                '-DGTK_DISABLE_DEPRECATION_WARNINGS',
                '-DGTK_PRINT_BACKEND_ENABLE_UNSUPPORTED',
              ],
              dependencies: libgtk_dep,
              install_dir: printbackends_install_dir,
              install : true)
