Prerequisites
=============

GTK requires the following packages:

 - Autotools or Meson

 - The GLib, Pango, GdkPixbuf, ATK and cairo libraries, available at
   the same location as GTK.

 - libepoxy, for cross-platform OpenGL support.
   It can be found here: https://github.com/anholt/libepoxy

 - Each GDK backend has its own backend-specific requirements. For
   the X11 backend, X11 R6 and XInput version 2 (as well as a number
   of other extensions) are required. The Wayland backend requires
   (obviously) the Wayland libraries.

 - gobject-introspection 

Simple install procedure for Meson
==================================

    $ tar xf gtk+-3.24.46.tar.xz                # unpack the sources
    $ cd gtk+-3.24.46                           # change to the toplevel directory
    $ meson setup _build                        # configure GTK+
    $ meson compile -C _build                   # build GTK+
    [ Become root if necessary ]
    # meson install -C _build                   # install GTK+

The Details
===========

Complete information about installing GTK+ and related libraries
can be found in the file:

- [gtk-building.html](./docs/reference/gtk/html/gtk-building.html)

Or online at:

- http://developer-old.gnome.org/gtk/3.24/gtk-building.html
