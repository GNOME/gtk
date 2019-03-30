#!/bin/sh

gtk_api_version=$1
gtk_abi_version=$2
gtk_libdir=$3
gtk_datadir=$4

# Package managers set this so we don't need to run
if [ -z "$DESTDIR" ]; then
  echo Compiling GSettings schemas...
  glib-compile-schemas ${gtk_datadir}/glib-2.0/schemas

  echo Updating desktop database...
  update-desktop-database -q ${gtk_datadir}/applications

  echo Updating icon cache...
  gtk-update-icon-cache -q -t -f ${gtk_datadir}/icons/hicolor

  echo Updating module cache for print backends...
  mkdir -p ${gtk_libdir}/gtk-3.0/3.0.0/printbackends
  gio-querymodules ${gtk_libdir}/gtk-3.0/3.0.0/printbackends

  echo Updating module cache for input methods...
  mkdir -p ${gtk_libdir}/gtk-3.0/3.0.0/immodules
  gio-querymodules ${gtk_libdir}/gtk-3.0/3.0.0/immodules
fi
