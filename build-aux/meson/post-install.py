#!/usr/bin/env python3

import os
import shutil
import sys
import subprocess

if 'DESTDIR' not in os.environ:
    gtk_api_version = sys.argv[1]
    gtk_abi_version = sys.argv[2]
    gtk_bindir = sys.argv[3]
    gtk_libdir = sys.argv[4]
    gtk_datadir = sys.argv[5]
    gtk_query_immodules = os.path.join(gtk_bindir, 'gtk-query-immodules-' + gtk_api_version)
    gtk_update_icon_cache = os.path.join(gtk_bindir, 'gtk-update-icon-cache')

    gtk_moduledir = os.path.join(gtk_libdir, 'gtk-' + gtk_api_version, gtk_abi_version)
    gtk_immodule_dir = os.path.join(gtk_moduledir, 'immodules')
    gtk_printmodule_dir = os.path.join(gtk_moduledir, 'printbackends')

    if os.name == 'nt':
        for lib in ['gdk', 'gtk', 'gailutil']:
            # Make copy for MSVC-built .lib files, e.g. xxx-3.lib->xxx-3.0.lib
            installed_lib = os.path.join(gtk_libdir, lib + '-' + gtk_api_version.split('.')[0] + '.lib')
            installed_lib_dst = os.path.join(gtk_libdir, lib + '-' + gtk_api_version + '.lib')
            if os.path.isfile(installed_lib):
                shutil.copyfile(installed_lib, installed_lib_dst)

    print('Compiling GSettings schemas...')
    glib_compile_schemas = subprocess.check_output(['pkg-config',
                                                   '--variable=glib_compile_schemas',
                                                   'gio-2.0']).strip()
    if not os.path.exists(glib_compile_schemas):
        # pkg-config variables only available since GLib 2.62.0.
        glib_compile_schemas = 'glib-compile-schemas'
    subprocess.call([glib_compile_schemas,
                    os.path.join(gtk_datadir, 'glib-2.0', 'schemas')])

    print('Updating icon cache...')
    subprocess.call([gtk_update_icon_cache, '-q', '-t' ,'-f',
                    os.path.join(gtk_datadir, 'icons', 'hicolor')])

    print('Updating module cache for input methods...')
    os.makedirs(gtk_immodule_dir, exist_ok=True)
    immodule_cache_file = open(os.path.join(gtk_moduledir, 'immodules.cache'), 'w')
    subprocess.call([gtk_query_immodules], stdout=immodule_cache_file)
    immodule_cache_file.close()

    # Untested!
    print('Updating module cache for print backends...')
    os.makedirs(gtk_printmodule_dir, exist_ok=True)
    gio_querymodules = subprocess.check_output(['pkg-config',
                                                '--variable=gio_querymodules',
                                                'gio-2.0']).strip()
    if not os.path.exists(gio_querymodules):
        # pkg-config variables only available since GLib 2.62.0.
        gio_querymodules = 'gio-querymodules'
    subprocess.call([gio_querymodules, gtk_printmodule_dir])
