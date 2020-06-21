#!/usr/bin/env python3

import os
import sys
import subprocess

if 'DESTDIR' not in os.environ:
    gtk_api_version = sys.argv[1]
    gtk_abi_version = sys.argv[2]
    gtk_libdir = sys.argv[3]
    gtk_datadir = sys.argv[4]

    gtk_moduledir = os.path.join(gtk_libdir, 'gtk-' + gtk_api_version, gtk_abi_version)
    gtk_printmodule_dir = os.path.join(gtk_moduledir, 'printbackends')
    gtk_immodule_dir = os.path.join(gtk_moduledir, 'immodules')

    print('Compiling GSettings schemas...')
    glib_compile_schemas = subprocess.check_output(['pkg-config',
                                                   '--variable=glib_compile_schemas',
                                                   'gio-2.0']).strip()
    subprocess.call([glib_compile_schemas,
                    os.path.join(gtk_datadir, 'glib-2.0', 'schemas')])

    print('Updating icon cache...')
    subprocess.call(['gtk4-update-icon-cache', '-q', '-t' ,'-f',
                    os.path.join(gtk_datadir, 'icons', 'hicolor')])

    print('Updating module cache for print backends...')
    os.makedirs(gtk_printmodule_dir, exist_ok=True)
    gio_querymodules = subprocess.check_output(['pkg-config',
                                                '--variable=gio_querymodules',
                                                'gio-2.0']).strip()
    subprocess.call([gio_querymodules, gtk_printmodule_dir])

    print('Updating module cache for input methods...')
    os.makedirs(gtk_immodule_dir, exist_ok=True)
    subprocess.call([gio_querymodules, gtk_immodule_dir])
