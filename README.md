GTK+ — The GTK toolkit
======================

General information
-------------------

GTK+ is a multi-platform toolkit for creating graphical user interfaces.
Offering a complete set of widgets, GTK+ is suitable for projects ranging
from small one-off projects to complete application suites.

GTK+ is free software and part of the GNU Project. However, the
licensing terms for GTK+, the GNU LGPL, allow it to be used by all
developers, including those developing proprietary software, without any
license fees or royalties.

The official download location

  - https://download.gnome.org/sources/gtk+

The official web site

  - https://www.gtk.org

The official developers blog

  - https://blog.gtk.org

Information about mailing lists can be found at

  - http://www.gtk.org/mailing-lists.php

Building and installing
-----------------------

In order to build GTK+ you will need:

  - a C99 compatible compiler
  - Python 3
  - [Meson](http://mesonbuild.com)
  - [Ninja](https://ninja-build.org)

You will also need various dependencies, based on the platform you are
building for:

  - [GLib](https://download.gnome.org/sources/glib)
  - [GdkPixbuf](https://download.gnome.org/sources/gdk-pixbuf)
  - [GObject-Introspection](https://download.gnome.org/sources/gobject-introspection)
  - [Cairo](https://www.cairographics.org)
  - [Pango](https://download.gnome.org/sources/pango)
  - [Epoxy](https://github.com/anholt/libepoxy)
  - [Graphene](https://github.com/ebassi/graphene)
  - [ATK](https://download.gnome.org/sources/atk)
  - [Xkb-common](https://github.com/xkbcommon/libxkbcommon)

If you are building the X11 backend, you will also need:

  - Xlib, and the following X extensions:
    - xrandr
    - xrender
    - xi
    - xext
    - xfixes (optional)
    - xcursor (optional)
    - xdamage (optional)
    - xcomposite (optional)
  - [atk-bridge-2.0](https://download.gnome.org/sources/at-spi2-atk)

If you are building the Wayland backend, you will also need:

  - Wayland-client
  - Wayland-protocols
  - Wayland-cursor
  - Wayland-EGL

Once you have all the necessary dependencies, you can build GTK+ by using
Meson:

```sh
$ meson _build .
$ cd _build
$ ninja
```

You can run the test suite using:

```sh
$ mesontest
```

And, finally, you can install GTK+ using:

```
$ sudo ninja install
```

Complete information about installing GTK+ and related libraries
can be found in the file:

```
docs/reference/gtk/html/gtk-building.html
```

Or [online](https://developer.gnome.org/gtk4/stable/gtk-building.html)

How to report bugs
------------------

Bugs should be reported to the GNOME [bug tracking system](https://bugzilla.gnome.org/enter_bug.cgi?product=gtk%2b).
You will need an account for yourself.

In the bug report please include:

* Information about your system. For instance:

   - which version of GTK+ you are using
   - what operating system and version
   - for Linux, which distribution
   - if you built GTK+, the list of options used to configure the build

  And anything else you think is relevant.

* How to reproduce the bug.

  If you can reproduce it with one of the test programs that are built
  in the tests/ subdirectory, that will be most convenient.  Otherwise,
  please include a short test program that exhibits the behavior.
  As a last resort, you can also provide a pointer to a larger piece
  of software that can be downloaded.

* If the bug was a crash, the exact text that was printed out
  when the crash occurred.

* Further information such as stack traces may be useful, but
  is not necessary.


Contributing
------------

Patches should also be submitted to the bug tracking system. If the patch
fixes an existing bug, add the patch as an attachment to that bug report;
otherwise, enter a new bug report that describes the patch, and attach the
patch to that bug report.

Patches should be in Git-formatted form. You should use `git format-patch`
to generate them. We recommend using [git-bz](http://git.fishsoup.net/man/git-bz.html).

For more information on the recommended workflow, please read
[this wiki page](https://wiki.gnome.org/Git/WorkingWithPatches).

Please, follow the `CODING_STYLE` document in order to conform to GTK+'s
coding style when submitting a code contribution.

Release notes
-------------

The release notes for GTK+ are part of the migration guide in the API
reference. See:

 - [3.x release notes](https://developer.gnome.org/gtk3/unstable/gtk-migrating-2-to-3.html)
 - [4.x release notes](https://developer.gnome.org/gtk4/unstable/gtk-migrating-3-to-4.html)

Licensing terms
---------------

GTK+ is released under the terms of the GNU Lesser General Public License,
version 2.1 or, at your option, any later version, as published by the Free
Software Foundation.

Please, see the `COPYING` file for further information.
