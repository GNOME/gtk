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
    subprocess.call(['glib-compile-schemas',
                    os.path.join(gtk_datadir, 'glib-2.0', 'schemas')])

    print('Updating icon cache...')
    subprocess.call(['gtk-update-icon-cache', '-q', '-t' ,'-f',
                    os.path.join(gtk_datadir, 'icons', 'hicolor')])

    print('Updating module cache for print backends...')
    if not os.path.isdir(gtk_printmodule_dir):
        os.mkdir(gtk_printmodule_dir)
    subprocess.call(['gio-querymodules', gtk_printmodule_dir])

    print('Updating module cache for input methods...')
    if not os.path.isdir(gtk_immodule_dir):
        os.mkdir(gtk_immodule_dir)
    subprocess.call(['gio-querymodules', gtk_immodule_dir])
