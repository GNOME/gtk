Title: Building GLib

# Building GLib 

GLib uses the [Meson build system](https://mesonbuild.com). The normal
sequence for compiling and installing the GLib library is thus:

    $ meson setup _build
    $ meson compile -C _build
    $ meson install -C _build

On FreeBSD, you will need something more complex:

    $ env CPPFLAGS="-I/usr/local/include" LDFLAGS="-L/usr/local/lib -Wl,--disable-new-dtags" \
    > meson setup \
    > -Dxattr=false \
    > -Dinstalled_tests=true \
    > -Diconv=external \
    > -Db_lundef=false \
    > _build
    $ meson compile -C _build

The standard options provided by Meson may be passed to the `meson` command. Please see the Meson documentation or run:

    meson configure --help

for information about the standard options.

GLib is compiled with strict aliasing disabled. It is strongly recommended
that this is not re-enabled by overriding the compiler flags, as GLib has
not been tested with strict aliasing and cannot be guaranteed to work.

## Dependencies

Before you can compile the GLib library, you need to have various other
tools and libraries installed on your system. If you are building from a
release archive, you will need a [compliant C
toolchain](https://wiki.gnome.org/Projects/GLib/CompilerRequirements),
Meson, and pkg-config; the requirements are the same when building from a
Git repository clone of GLib.

- [`pkg-config`](https://www.freedesktop.org/wiki/Software/pkg-config/) is a
  tool for tracking the compilation flags needed for libraries that are used
  by the GLib library. (For each library, a small `.pc` text file is
  installed in a standard location that contains the compilation flags
  needed for that library along with version number information). 

A UNIX build of GLib requires that the system implements at least the
original 1990 version of POSIX. Beyond this, it depends on a number of other
libraries.

- The [GNU libiconv library](http://www.gnu.org/software/libiconv/) is
  needed to build GLib if your system doesn't have the `iconv()` function
  for doing conversion between character encodings. Most modern systems
  should have `iconv()`, however many older systems lack an `iconv()`
  implementation. On such systems, you must install the libiconv library.
  This can be found at: http://www.gnu.org/software/libiconv.

  If your system has an `iconv()` implementation but you want to use
  `libiconv` instead, you can pass the `-Diconv=gnu` option to Meson. This
  forces libiconv to be used.

  Note that if you have libiconv installed in your default include search
  path (for instance, in /usr/local/), but don't enable it, you will get an
  error while compiling GLib because the `iconv.h` that libiconv installs
  hides the system iconv.

  If you are using the native iconv implementation on Solaris instead of
  libiconv, you'll need to make sure that you have the converters between
  locale encodings and UTF-8 installed. At a minimum you'll need the
  SUNWuiu8 package. You probably should also install the SUNWciu8, SUNWhiu8,
  SUNWjiu8, and SUNWkiu8 packages.

  The native iconv on Compaq Tru64 doesn't contain support for UTF-8, so
  you'll need to use GNU libiconv instead. (When using GNU libiconv for
  GLib, you'll need to use GNU libiconv for GNU gettext as well.) This
  probably applies to related operating systems as well.

- Python 3.5 or newer is required. Your system Python must conform to PEP
  394 For FreeBSD, this means that the lang/python3 port must be installed.

- The libintl library from the [GNU
  gettext](http://www.gnu.org/software/gettext) package is needed if your
  system doesn't have the `gettext()` functionality for handling message
  translation databases.

- A thread implementation is needed. The thread support in GLib can be based
  upon POSIX threads or win32 threads.

- GRegex uses the [PCRE library](http://www.pcre.org/) for regular
  expression matching. The default is to use the system version of PCRE, to
  reduce the chances of security fixes going out of sync. GLib additionally
  provides an internal copy of PCRE in case the system version is too old,
  or does not support UTF-8; the internal copy is patched to use GLib for
  memory management and to share the same Unicode tables.

- The optional extended attribute support in GIO requires the `getxattr()`
  family of functions that may be provided by the C library or by the
  standalone libattr library. To build GLib without extended attribute
  support, use the `-Dxattr=false` option.

- The optional SELinux support in GIO requires libselinux. To build GLib
  without SELinux support, use the `-Dselinux=disabled` option.

- The optional support for DTrace requires the `sys/sdt.h` header, which is
  provided by SystemTap on Linux. To build GLib without DTrace, use the
  `-Ddtrace=false` option.

- The optional support for SystemTap can be disabled with the
  `-Dsystemtap=false` option. Additionally, you can control the location
  where GLib installs the SystemTap probes, using the
  `-Dtapset_install_dir=DIR` option.

## Extra Configuration Options

In addition to the normal options, these additional ones are supported when
configuring the GLib library:

`--buildtype`
: This is a standard Meson option which specifies how much debugging and
  optimization to enable. If the build type starts with `debug`,
  `G_ENABLE_DEBUG` will be defined and GLib will be built with additional
  debug code enabled. If the build type is `plain`, GLib will not enable any
  optimization or debug options by default, and will leave it entirely to
  the user to choose their options. To build with the options recommended
  by GLib developers, choose `release`.

`-Dforce_posix_threads=true`
: Normally, Meson should be able to work out the correct thread implementation
  to use. This option forces POSIX threads to be used even if the platform
  provides another threading API (for example, on Windows).

`-Dinternal_pcre=true`
: Normally, GLib will be configured to use the system-supplied PCRE library if
  it is suitable, falling back to an internal version otherwise. If this
  option is specified, the internal version will always be used. Using the
  internal PCRE is the preferred solution if:

  - your system has strict resource constraints; the system-supplied PCRE
    has a separated copy of the tables used for Unicode handling, whereas
    the internal copy shares the Unicode tables used by GLib.
  - your system has PCRE built without some needed features, such as UTF-8
    and Unicode support.
  - you are planning to use both GRegex and PCRE API at the same time,
    either directly or indirectly through a dependency; PCRE uses some
    global variables for memory management and other features, and if both
    GLib and PCRE try to access them at the same time, this could lead to
    undefined behavior.

`-Dbsymbolic_functions=false` and `-Dbsymbolic_functions=true`
: By default, GLib uses the `-Bsymbolic-functions` linker flag to avoid
  intra-library PLT jumps. A side-effect of this is that it is no longer
  possible to override internal uses of GLib functions with `LD_PRELOAD`.
  Therefore, it may make sense to turn this feature off in some
  situations. The `-Dbsymbolic_functions=false` option allows to do that.

`-Dgtk_doc=false` and `-Dgtk_doc=true`
: By default, GLib will detect whether the gtk-doc package is installed.
  If it is, then it will use it to extract and build the documentation
  for the GLib library. These options can be used to explicitly control
  whether gtk-doc should be used or not. If it is not used, the
  distributed, pre-generated HTML files will be installed instead of
  building them on your machine.

`-Dman=false` and `-Dman=true`
: By default, GLib will detect whether xsltproc and the necessary DocBook
  stylesheets are installed. If they are, then it will use them to rebuild
  the included man pages from the XML sources. These options can be used to
  explicitly control whether man pages should be rebuilt used or not. The
  distribution includes pre-generated man pages.

`-Dxattr=false` and `-Dxattr=true`
: By default, GLib will detect whether the `getxattr()` family of functions is
  available. If it is, then extended attribute support will be included in
  GIO. These options can be used to explicitly control whether extended
  attribute support should be included or not. `getxattr()` and friends can be
  provided by glibc or by the standalone libattr library.

`-Dselinux=auto`, `-Dselinux=enabled` or `-Dselinux=disabled`
: By default, GLib will detect if libselinux is available and include SELinux
  support in GIO if it is. These options can be used to explicitly control
  whether SELinux support should be included.

`-Ddtrace=false` and `-Ddtrace=true`
: By default, GLib will detect if DTrace support is available, and use it.
  These options can be used to explicitly control whether DTrace support is
  compiled into GLib.

`-Dsystemtap=false` and `-Dsystemtap=true`
: This option requires DTrace support. If it is available, then GLib will also
  check for the presence of SystemTap.

`-Db_coverage=true` and `-Db_coverage=false`
: Enable the generation of coverage reports for the GLib tests. This requires
  the lcov frontend to gcov from the Linux Test Project. To generate a
  coverage report, use `ninja coverage-html`. The report is placed in the
  `meson-logs` directory.

`-Druntime_libdir=RELPATH`
: Allows specifying a relative path to where to install the runtime libraries
  (meaning library files used for running, not developing, GLib applications).
  This can be used in operating system setups where programs using GLib needs
  to run before e.g. `/usr` is mounted. For example, if LIBDIR is `/usr/lib`
  and `../../lib` is passed to `-Druntime_libdir` then the runtime libraries
  are installed into `/lib` rather than `/usr/lib`.
