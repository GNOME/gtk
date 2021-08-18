Title: Compiling the GTK libraries

## Building GTK on UNIX-like systems

This chapter covers building and installing GTK on UNIX and UNIX-like
systems such as Linux. Compiling GTK on Microsoft Windows is different in
detail and somewhat more difficult to get going since the necessary tools
aren't included with the operating system.

Before we get into the details of how to compile GTK, we should mention that
in many cases, binary packages of GTK prebuilt for your operating system
will be available, either from your operating system vendor or from
independent sources. If such a set of packages is available, installing it
will get you programming with GTK much faster than building it yourself. In
fact, you may well already have GTK installed on your system already.

On UNIX-like systems GTK uses the standard GNU build system, using autoconf
for package configuration and resolving portability issues, automake for
building makefiles that comply with the GNU Coding Standards, and libtool
for building shared libraries on multiple platforms.

If you are building GTK from the distributed source packages, then you won't
need these tools installed; the necessary pieces of the tools are already
included in the source packages. But it's useful to know a bit about how
packages that use these tools work. A source package is distributed as a
tar.bz2 or tar.xz file which you unpack into a directory full of the source
files as follows:

    tar xvfj gtk+-3.24.0.tar.bz2
    tar xvfJ gtk+-3.24.0.tar.xz

In the toplevel directory that is created, there will be a shell script
called `configure` which you then run to take the template makefiles called
`Makefile.in` in the package and create makefiles customized for your
operating system. The configure script can be passed various command line
arguments to determine how the package is built and installed. The most
commonly useful argument is the `--prefix` argument which determines where the
package is installed. To install a package in `/opt/gtk` you would run
configure as:

    ./configure --prefix=/opt/gtk

A full list of options can be found by running configure with the `--help`
argument. In general, the defaults are right and should be trusted. After
you've run configure, you then run the make command to build the package and
install it:

    make
    make install

If you don't have permission to write to the directory you are installing
in, you may have to change to root temporarily before running make install.
Also, if you are installing in a system directory, on some systems (such as
Linux), you will need to run `ldconfig` after `make install` so that the
newly installed libraries will be found.

Several environment variables are useful to pass to set before running
configure. `CPPFLAGS` contains options to pass to the C preprocessor, and is
used to tell the compiler where to look for include files. `CFLAGS` contains
options to pass to the C ompiler. The `LDFLAGS` variable is used in a
similar fashion for the linker. Finally the `PKG_CONFIG_PATH` environment
variable contains a search path that pkg-config (see below) uses when
looking for files describing how to compile programs using different
libraries. If you were installing GTK and it's dependencies into `/opt/gtk`,
you might want to set these variables as:

    CPPFLAGS="-I/opt/gtk/include"
    LDFLAGS="-L/opt/gtk/lib"
    PKG_CONFIG_PATH="/opt/gtk/lib/pkgconfig"
    export CPPFLAGS LDFLAGS PKG_CONFIG_PATH

You may also need to set the `LD_LIBRARY_PATH` environment variable so the
systems dynamic linker can find the newly installed libraries, and the
`PATH` environment program so that utility binaries installed by the various
libraries will be found.

    export LD_LIBRARY_PATH="/opt/gtk/lib"
    export PATH="/opt/gtk/bin:$PATH"

### Dependencies

Before you can compile the GTK widget toolkit, you need to have various
other tools and libraries installed on your system. The two tools needed
during the build process (as differentiated from the tools used in when
creating GTK mentioned above such as autoconf) are `pkg-config` and `GNU make`.

- [pkg-config](https://www.freedesktop.org/wiki/Software/pkg-config/) is a
  tool for tracking the compilation flags needed for libraries that are used
  by the GTK libraries. (For each library, a small .pc text file is
  installed in a standard location that contains the compilation flags
  needed for that library along with version number information.)

- The GTK makefiles will mostly work with different versions of make,
  however, there tends to be a few incompatibilities, so the GTK team
  recommends installing [GNU make](https://www.gnu.org/software/make) if you
  don't already have it on your system and using it. (It may be called gmake
  rather than make.)

Some of the libraries that GTK depends on are maintained by by the GTK team:
GLib, GdkPixbuf, Pango, ATK and GObject Introspection. Other libraries are
maintained separately.

- The GLib library provides core non-graphical functionality such as high
  level data types, Unicode manipulation, and an object and type system to C
  programs. It is available from the [GNOME file
  server](https://download.gnome.org/sources/glib/).
- The GdkPixbuf library provides facilities for loading images in a variety
  of file formats. It is available from the [GNOME file
  server](https://download.gnome.org/sources/gdk-pixbuf/).
- Pango is a library for internationalized text handling. It is available
  from the [GNOME file server](https://download.gnome.org/sources/pango/).
- ATK is the Accessibility Toolkit. It provides a set of generic interfaces
  allowing accessibility technologies such as screen readers to interact
  with a graphical user interface. It is available from the [GNOME file
  server](https://download.gnome.org/sources/atk/).
- Gobject Introspection is a framework for making introspection data
  available to language bindings. It is available from the [GNOME file
  server](https://download.gnome.org/sources/gobject-introspection/).

### External dependencies

- The GNU libiconv library is needed to build GLib if your system doesn't
  have the `iconv()` function for doing conversion between character
  encodings. Most modern systems should have `iconv()`.
- The libintl library from the GNU gettext package is needed if your system
  doesn't have the `gettext()` functionality for handling message
  translation databases.
- The libraries from the X window system are needed to build Pango and GTK.
  You should already have these installed on your system, but it's possible
  that you'll need to install the development environment for these
  libraries that your operating system vendor provides.
- The fontconfig library provides Pango with a standard way of locating
  fonts and matching them against font names.
- Cairo is a graphics library that supports vector graphics and image
  compositing. Both Pango and GTK use cairo for all of their drawing.
- libepoxy is a library that abstracts the differences between different
  OpenGL libraries. GTK uses it for cross-platform GL support.
- The Wayland libraries are needed to build GTK with the Wayland backend.
- The shared-mime-info package is not a hard dependency of GTK, but it
  contains definitions for mime types that are used by GIO and, indirectly,
  by GTK. gdk-pixbuf will use GIO for mime type detection if possible. For
  this to work, shared-mime-info needs to be installed and `XDG_DATA_DIRS`
  set accordingly at configure time. Otherwise, gdk-pixbuf falls back to its
  built-in mime type detection.

### Building and testing GTK

First make sure that you have the necessary external dependencies installed:
pkg-config, GNU make, the JPEG, PNG, and TIFF libraries, FreeType, and, if
necessary, libiconv and libintl. To get detailed information about building
these packages, see the documentation provided with the individual packages.
On a Linux system, it's quite likely you'll have all of these installed
already except for pkg-config.

Then build and install the GTK libraries in the order: GLib, Pango, ATK,
then GTK. For each library, follow the steps of configure, make, make
install mentioned above. If you're lucky, this will all go smoothly, and
you'll be ready to start compiling your own GTK applications. You can test
your GTK installation by running the gtk3-demo program that GTK installs.

If one of the `configure` scripts fails or running make fails, look closely at
the error messages printed; these will often provide useful information as
to what went wrong. When `configure` fails, extra information, such as errors
that a test compilation ran into, is found in the file config.log. Looking
at the last couple of hundred lines in this file will frequently make clear
what went wrong. If all else fails, you can ask for help on GNOME's
[Discourse](https://discourse.gnome.org/tag/gtk).

### Extra Configuration Options

In addition to the normal options, the configure script for the GTK library
supports a number of additional arguments. (Command line arguments for the
other GTK libraries are described in the documentation distributed with the
those libraries.)

```
configure
  [ --disable-modules | --enable-modules ]
  [[--with-included-immodules=MODULE1,MODULE2,...]]
  [ --enable-debug=[no/minimum/yes] ]
  [ --disable-Bsymbolic | --enable-Bsymbolic ]
  [ --disable-xkb | --enable-xkb ]
  [ --disable-xinerama | --enable-xinerama ]
  [ --disable-gtk-doc | --enable-gtk-doc ]
  [ --disable-cups | --enable-cups ]
  [ --disable-papi | --enable-papi ]
  [ --enable-xinput | --disable-xinput ]
  [ --enable-packagekit | --disable-packagekit ]
  [ --enable-x11-backend | --disable-x11-backend ]
  [ --enable-win32-backend | --disable-win32-backend ]
  [ --enable-quartz-backend | --disable-quartz-backend ]
  [ --enable-broadway-backend | --disable-broadway-backend ]
  [ --enable-wayland-backend | --disable-wayland-backend ]
  [ --enable-introspection=[no/auto/yes] ]
  [ --enable-installed-tests | --disable-installed-tests ]
```

`--disable-modules` and `--enable-modules`
: Normally GTK will try to build the input method modules as little shared
  libraries that are loaded on demand. The `--disable-modules` argument
  indicates that they should all be built statically into the GTK library
  instead. This is useful for people who need to produce statically-linked
  binaries. If neither `--disable-modules` nor `--enable-modules` is
  specified, then the `configure` script will try to auto-detect whether
  shared modules work on your system.

`--with-included-immodules`
: This option allows you to specify which input method modules you want to
  include directly into the GTK shared library, as opposed to building them
  as loadable modules.

`--enable-debug`
: Turns on various amounts of debugging support. Setting this to `no` disables
  `g_assert()`, `g_return_if_fail()`, `g_return_val_if_fail()` and all cast
  checks between different object types. Setting it to `minimum` disables only
  cast checks. Setting it to `yes` enables runtime debugging. The default is
  `minimum`. Note that 'no' is fast, but dangerous as it tends to destabilize
  even mostly bug-free software by changing the effect of many bugs from
  simple warnings into fatal crashes. Thus `--enable-debug=no` should not be
  used for stable releases of GTK unless you know precisely what you're
  doing.

`--disable-Bsymbolic` and `--enable-Bsymbolic`
: The option `--disable-Bsymbolic` turns off the use of the
  `-Bsymbolic-functions` linker flag. This is only necessary if you want to
  override GTK functions by using `LD_PRELOAD`.

`--enable-explicit-deps` and `--disable-explicit-deps`
: If `--enable-explicit-deps` is specified then GTK will write the full set
  of libraries that GTK depends upon into its .pc files to be used when
  programs depending on GTK are linked. Otherwise, GTK only will include the
  GTK libraries themselves, and will depend on system library dependency
  facilities to bring in the other libraries. By default GTK will disable
  explicit dependencies unless it detects that they are needed on the system.
  (If you specify `--enable-static` to force building of static libraries,
  then explicit dependencies will be written since library dependencies don't
  work for static libraries.) Specifying `--enable-explicit-deps` or
  `--enable-static` can cause compatibility problems when libraries that GTK
  depends upon change their versions, and should be avoided if possible.

`--disable-xkb` and `--enable-xkb`
: By default the configure script will try to auto-detect whether the XKB
  extension is supported by the X libraries GTK is linked with. These options
  can be used to explicitly control whether GTK will support the XKB extension.

`--disable-xinerama` and `--enable-xinerama`
: By default the configure script will try to link against the Xinerama
  libraries if they are found. These options can be used to explicitly control
  whether Xinerama should be used.

`--disable-xinput` and `--enable-xinput`
: Controls whether GTK is built with support for the XInput or XInput2
  extension. These extensions provide an extended interface to input devices
  such as touchscreens, touchpads, and graphics tablets. When this support is
  compiled in, specially written GTK programs can get access to subpixel
  positions, multiple simultaneous input devices, and extra "axes" provided by
  the device such as pressure and tilt information.

`--disable-gtk-doc` and `--enable-gtk-doc`
: The gtk-doc package is used to generate the reference documentation included
  with GTK. By default support for gtk-doc is disabled because it requires
  various extra dependencies to be installed. If you have gtk-doc installed and
  are modifying GTK, you may want to enable gtk-doc support by passing in
  `--enable-gtk-doc`. If not enabled, pre-generated HTML files distributed with
  GTK will be installed.

`--disable-cups` and `--enable-cups`
: By default the configure script will try to build the CUPS print backend if
  the CUPS libraries are found. These options can be used to explicitly control
  whether the cups print backend should be built.

`--disable-papi` and `--enable-papi`
: By default the configure script will try to build the PAPI print backend if
  the PAPI libraries are found. These options can be used to explicitly control
  whether the PAPI print backend should be built.

`--disable-packagekit` and `--enable-packagekit`
: By default the configure script will try to build the PackageKit support for
  the open-with dialog if the PackageKit libraries are found. These options can
  be used to explicitly control whether PackageKit support should be built.

`--enable-x11-backend`, `--disable-x11-backend`
: Enables the X11 backend for GDK. The X11 backend is enabled by default on
  Unix and Unix-like systems like Linux.

`--enable-win32-backend`, `--disable-win32-backend`
: Enables the Windows backend for GDK. The Windows backend is enabled by
  default on Windows.

`--enable-quartz-backend`, `--disable-quartz-backend`
: Enables the Quartz backend for GDK. The Quartz backend is enabled by default
  on macOS.

`--enable-broadway-backend`, `--disable-broadway-backend`
: Enables the Broadway backend for GDK.

`--enable-wayland-backend`, `--disable-wayland-backend`
: Enables the Wayland backend for GDK. The Wayland backend is enabled by
  default on Linux.

`--enable-introspection`
: Build with or without introspection support. The default is 'auto'.

`--enable-installed-tests` or `--disable-installed-tests`
: Whether to install tests on the system. If enabled, tests and their data are
  installed in `${libexecdir}/gtk+/installed-tests`. Metadata for the tests is
  installed in `${prefix}/share/installed-tests/gtk+`. To run the installed
  tests, you can use the
  [gnome-desktop-testing-runner](https://gitlab.gnome.org/GNOME/gnome-desktop-testing)
  harness.

