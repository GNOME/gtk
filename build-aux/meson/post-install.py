#!/usr/bin/env python3

import os
import sys
import subprocess

if 'DESTDIR' not in os.environ:
    gtk_api_version = sys.argv[1]
    gtk_abi_version = sys.argv[2]
    gtk_bindir = sys.argv[3]
    gtk_libdir = sys.argv[4]
    gtk_datadir = sys.argv[5]
    gtk_query_immodules = os.path.join(gtk_bindir, 'gtk-query-immodules-' + gtk_api_version)

    gtk_moduledir = os.path.join(gtk_libdir, 'gtk-' + gtk_api_version, gtk_abi_version)
    gtk_immodule_dir = os.path.join(gtk_moduledir, 'immodules')
    gtk_printmodule_dir = os.path.join(gtk_moduledir, 'printbackends')

    print('Compiling GSettings schemas...')
    subprocess.call(['glib-compile-schemas',
                    os.path.join(gtk_datadir, 'glib-2.0', 'schemas')])

    print('Updating icon cache...')
    subprocess.call(['gtk-update-icon-cache', '-q', '-t' ,'-f',
                    os.path.join(gtk_datadir, 'icons', 'hicolor')])

    print('Updating module cache for input methods...')
    os.makedirs(gtk_immodule_dir, exist_ok=True)
    immodule_cache_file = open(os.path.join(gtk_moduledir, 'immodules.cache'), 'w')
    subprocess.call([gtk_query_immodules], stdout=immodule_cache_file)
    immodule_cache_file.close()

    # Untested!
    print('Updating module cache for print backends...')
    os.makedirs(gtk_printmodule_dir, exist_ok=True)
    subprocess.call(['gio-querymodules', gtk_printmodule_dir])
