#!/bin/sh

TARGET=$1

meson setup  _build || exit $?
meson compile -C _build || exit $?

mv _build/glib/glib/glib/ ${TARGET}/glib/
mv _build/glib/gmodule/gmodule/ ${TARGET}/gmodule/
mv _build/glib/gobject/gobject/ ${TARGET}/gobject/
mv _build/glib/gio/gio/ ${TARGET}/gio/

mv _build/gtk3/gdk/gdk3/ ${TARGET}/gdk3/
mv _build/gtk3/gdk/gdk3-x11/ ${TARGET}/gdk3-x11/
mv _build/gtk3/gtk/gtk3/ ${TARGET}/gtk3/

mv _build/atk/atk/atk/ ${TARGET}/atk/

mv _build/atk/atspi2/atspi2/ ${TARGET}/atspi2/
