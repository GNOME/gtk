# Process this file with autoconf to produce a configure script.

# Making releases:
#   GTK_MICRO_VERSION += 1;
#   GTK_INTERFACE_AGE += 1;
#   GTK_BINARY_AGE += 1;
# if any functions have been added, set GTK_INTERFACE_AGE to 0.
# if backwards compatibility has been broken,
# set GTK_BINARY_AGE and GTK_INTERFACE_AGE to 0.

m4_define([gtk_major_version], [3])
m4_define([gtk_minor_version], [14])
m4_define([gtk_micro_version], [13])
m4_define([gtk_interface_age], [13])
m4_define([gtk_binary_age],
          [m4_eval(100 * gtk_minor_version + gtk_micro_version)])
m4_define([gtk_version],
          [gtk_major_version.gtk_minor_version.gtk_micro_version])
# This is the X.Y used in -lgtk-FOO-X.Y
m4_define([gtk_api_version], [3.0])

AC_PREREQ([2.62])
AC_INIT([gtk+], [gtk_version],
        [http://bugzilla.gnome.org/enter_bug.cgi?product=gtk%2B],
        [gtk+])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([gdk/gdktypes.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])

AC_CANONICAL_HOST

# Define a string for the earliest version that this release has
# backwards binary compatibility with for all interfaces a module
# might. Unless we add module-only API with lower stability
# guarantees, this should be unchanged until we break binary compat
# for GTK+.
#
#GTK_BINARY_VERSION=$GTK_MAJOR_VERSION.$GTK_MINOR_VERSION.$LT_CURRENT
m4_define([gtk_binary_version], [3.0.0])

# required versions of other packages
m4_define([glib_required_version], [2.41.2])
m4_define([pango_required_version], [1.36.7])
m4_define([atk_required_version], [2.12.0])
m4_define([cairo_required_version], [1.12.0])
m4_define([gdk_pixbuf_required_version], [2.30.0])
m4_define([introspection_required_version], [1.39.0])
m4_define([wayland_required_version], [1.5.91])
GLIB_REQUIRED_VERSION=glib_required_version
PANGO_REQUIRED_VERSION=pango_required_version
ATK_REQUIRED_VERSION=atk_required_version
CAIRO_REQUIRED_VERSION=cairo_required_version
GDK_PIXBUF_REQUIRED_VERSION=gdk_pixbuf_required_version
INTROSPECTION_REQUIRED_VERSION=introspection_required_version

AC_SUBST(GLIB_REQUIRED_VERSION)
AC_SUBST(PANGO_REQUIRED_VERSION)
AC_SUBST(ATK_REQUIRED_VERSION)
AC_SUBST(CAIRO_REQUIRED_VERSION)
AC_SUBST(GDK_PIXBUF_REQUIRED_VERSION)
AC_SUBST(INTROSPECTION_REQUIRED_VERSION)

# Save this value here, since automake will set cflags later
cflags_set=${CFLAGS+set}

AM_INIT_AUTOMAKE([1.11 -Wall no-define -Wno-portability tar-ustar no-dist-gzip dist-xz])
AM_MAINTAINER_MODE([enable])

# Support silent build rules. Disable
# by either passing --disable-silent-rules to configure or passing V=1
# to make
AM_SILENT_RULES([yes])

#
# For each of the libraries we build, we define the following
# substituted variables:
#
# foo_PACKAGES:     pkg-config packages this library requires
# foo_EXTRA_LIBS:   Libraries this module requires not pulled in by pkg-config
# foo_EXTRA_CFLAGS: cflags this module requires not pulled in by pkg-config
# foo_DEP_LIBS:     All libraries this module requires
# foo_DEP_CFLAGS:   All cflags this module requires


GTK_MAJOR_VERSION=gtk_major_version
GTK_MINOR_VERSION=gtk_minor_version
GTK_MICRO_VERSION=gtk_micro_version
GTK_INTERFACE_AGE=gtk_interface_age
GTK_BINARY_AGE=gtk_binary_age
GTK_VERSION=gtk_version
GTK_API_VERSION=gtk_api_version
GTK_BINARY_VERSION=gtk_binary_version
AC_SUBST(GTK_MAJOR_VERSION)
AC_SUBST(GTK_MINOR_VERSION)
AC_SUBST(GTK_MICRO_VERSION)
AC_SUBST(GTK_INTERFACE_AGE)
AC_SUBST(GTK_BINARY_AGE)
AC_SUBST(GTK_API_VERSION)
AC_SUBST(GTK_VERSION)
AC_SUBST(GTK_BINARY_VERSION)

# libtool versioning
#LT_RELEASE=$GTK_MAJOR_VERSION.$GTK_MINOR_VERSION
#LT_CURRENT=`expr $GTK_MICRO_VERSION - $GTK_INTERFACE_AGE`
#LT_REVISION=$GTK_INTERFACE_AGE
#LT_AGE=`expr $GTK_BINARY_AGE - $GTK_INTERFACE_AGE`
#LT_CURRENT_MINUS_AGE=`expr $LT_CURRENT - $LT_AGE`

m4_define([lt_current], [m4_eval(100 * gtk_minor_version + gtk_micro_version - gtk_interface_age)])
m4_define([lt_revision], [gtk_interface_age])
m4_define([lt_age], [m4_eval(gtk_binary_age - gtk_interface_age)])
LT_VERSION_INFO="lt_current:lt_revision:lt_age"
LT_CURRENT_MINUS_AGE=m4_eval(lt_current - lt_age)
AC_SUBST(LT_VERSION_INFO)
AC_SUBST(LT_CURRENT_MINUS_AGE)

m4_define([gail_lt_current],[0])
m4_define([gail_lt_revision],[0])
m4_define([gail_lt_age],[0])
m4_define([gail_lt_version_info],[gail_lt_current:gail_lt_revision:gail_lt_age])
m4_define([gail_lt_current_minus_age],[m4_eval(gail_lt_current - gail_lt_age)])
AC_SUBST([GAIL_LT_VERSION_INFO],[gail_lt_version_info])
AC_SUBST([GAIL_LT_CURRENT_MINUS_AGE],[gail_lt_current_minus_age])

# Workaround for https://bugzilla.gnome.org/show_bug.cgi?id=721346 until fixed upstream in autoconf-archive
if test "$cross_compiling" = "no"; then
  if test -n "$CFLAGS"; then
    CFLAGS_FOR_BUILD=$CFLAGS
  fi
  if test -n "$CPPFLAGS"; then
    CPPFLAGS_FOR_BUILD=$CPPFLAGS
  fi
  if test -n "$LDFLAGS"; then
    LDFLAGS_FOR_BUILD=$LDFLAGS
  fi
fi

# Checks for programs.
AC_PROG_CC
AX_PROG_CC_FOR_BUILD
AC_PROG_CC_C_O
AC_PROG_MKDIR_P
AC_PROG_INSTALL
AC_PROG_MAKE_SET


GETTEXT_PACKAGE=gtk30
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE",
                   [The prefix for our gettext translation domains.])

MATH_LIB=-lm
AC_MSG_CHECKING([for native Win32])
LIB_EXE_MACHINE_FLAG=X86
EXE_MANIFEST_ARCHITECTURE=X86
DISABLE_ON_W32=''
case "$host" in
  *-*-mingw*)
    dnl Comment-out certain gtk3.types.in lines
    DISABLE_ON_W32='%'
    os_win32=yes
    gio_can_sniff=no
    MATH_LIB=
    case "$host" in
    x86_64-*-*)
      LIB_EXE_MACHINE_FLAG=X64
      EXE_MANIFEST_ARCHITECTURE=AMD64
      ;;
    esac
    ;;
  *)
    os_win32=no
    ;;
esac
AC_MSG_RESULT([$os_win32])

AC_SUBST(DISABLE_ON_W32)
AC_SUBST(LIB_EXE_MACHINE_FLAG)
AC_SUBST(EXE_MANIFEST_ARCHITECTURE)

case $host in
  *-*-linux*)
    os_linux=yes
    ;;
  *-*-darwin*)
    os_darwin=yes
    ;;
esac

# Define _GNU_SOURCE etc. where appropriate, e.g. for strptime() on glibc
AC_USE_SYSTEM_EXTENSIONS

dnl
dnl Check for a working C++ compiler, but do not bail out, if none is found.
dnl We use this for an automated test for C++ header correctness.
dnl
AC_CHECK_TOOLS(CXX, [$CCC c++ g++ gcc CC cxx cc++ cl], gcc)
AC_LANG_PUSH([C++])

AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]],
                                   [[class a { int b; } c;]])],
                  [],[CXX=])
AM_CONDITIONAL(HAVE_CXX, test "$CXX" != "")

gtk_save_cxxflags="$CXXFLAGS"
CXXFLAGS="$CXXFLAGS -x objective-c++"
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[@interface Foo @end]],
                                   [[]])],
                  [OBJC=yes],[OBJC=no])
AM_CONDITIONAL(HAVE_OBJC, test "$OBJC" = "yes")
CXXFLAGS="$gtk_save_cxxflags"
AC_LANG_POP([C++])

if test "$os_win32" = "yes"; then
  if test x$enable_static = xyes -o x$enable_static = x; then
    AC_MSG_WARN([Disabling static library build, must build as DLL on Windows.])
    enable_static=no
  fi
  if test x$enable_shared = xno; then
    AC_MSG_WARN([Enabling shared library build, must build as DLL on Windows.])
  fi
  enable_shared=yes
fi

dnl Initialize libtool
LT_PREREQ([2.2.6])
LT_INIT([disable-static win32-dll])
# Create libtool early, because it's used in configure
LT_OUTPUT

# Make sure we use 64-bit versions of various file stuff.
AC_SYS_LARGEFILE

AM_PROG_AS
AC_PATH_TOOL(NM, nm, nm)

AC_MSG_CHECKING([for some Win32 platform])
case "$host" in
  *-*-mingw*|*-*-cygwin*)
    platform_win32=yes
    ;;
  *)
    platform_win32=no
    ;;
esac
AC_MSG_RESULT([$platform_win32])
AM_CONDITIONAL(PLATFORM_WIN32, test "$platform_win32" = "yes")

AM_CONDITIONAL(OS_WIN32, test "$os_win32" = "yes")
AM_CONDITIONAL(OS_UNIX, test "$os_win32" != "yes")
AM_CONDITIONAL(OS_LINUX, test "$os_linux" = "yes")
AM_CONDITIONAL(OS_DARWIN, test "$os_darwin" = "yes")

if test "$os_win32" = "yes"; then
  AC_CHECK_TOOL(WINDRES, windres, no)
  if test "$WINDRES" = no; then
    AC_MSG_ERROR([*** Could not find an implementation of windres in your PATH.])
  fi
  AC_CHECK_PROG(ms_librarian, lib.exe, yes, no)
fi
AM_CONDITIONAL(MS_LIB_AVAILABLE, test x$ms_librarian = xyes)

m4_define([debug_default],
          m4_if(m4_eval(gtk_minor_version % 2), [1], [yes], [minimum]))

dnl declare --enable-* args and collect ac_help strings
AC_ARG_ENABLE(debug,
              [AS_HELP_STRING([--enable-debug=@<:@no/minimum/yes@:>@],
                              [turn on debugging @<:@default=debug_default@:>@])],,
              [enable_debug=debug_default])

AC_ARG_ENABLE(installed_tests,
              AS_HELP_STRING([--enable-installed-tests],
                             [Install test programs (default: no)]),,
              [enable_installed_tests=no])
AM_CONDITIONAL(BUILDOPT_INSTALL_TESTS, test x$enable_installed_tests = xyes)

AC_ARG_ENABLE(gtk2-dependency,
              [AS_HELP_STRING([--enable-gtk2-dependency],
                              [Do not build gtk-update-icon-cache and other shared tools])],,
              [enable_gtk2_dependency=no])

AM_CONDITIONAL(BUILD_ICON_CACHE, [test "x$enable_gtk2_dependency" = xno])

AC_ARG_ENABLE(xkb,
              [AS_HELP_STRING([--enable-xkb],
                              [support XKB extension [default=maybe]])],,
              [enable_xkb="maybe"])
AC_ARG_ENABLE(xinerama,
              [AS_HELP_STRING([--enable-xinerama],
	                      [support Xinerama extension [default=maybe]])],,
              [enable_xinerama="maybe"])
AC_ARG_ENABLE(xrandr,
              [AS_HELP_STRING([--enable-xrandr],
                              [support XRandR extension [default=maybe]])],,
              [enable_xrandr="maybe"])
AC_ARG_ENABLE(xfixes,
              [AS_HELP_STRING([--enable-xfixes],
                              [support XFixes extension [default=maybe]])],,
              [enable_xfixes="maybe"])
AC_ARG_ENABLE(xcomposite,
              [AS_HELP_STRING([--enable-xcomposite],
                              [support X Composite extension [default=maybe]])],,
              [enable_xcomposite="maybe"])
AC_ARG_ENABLE(xdamage,
              [AS_HELP_STRING([--enable-xdamage],
                              [support X Damage extension [default=maybe]])],,
              [enable_xdamage="maybe"])

AC_ARG_ENABLE(x11-backend,
              [AS_HELP_STRING([--enable-x11-backend],
                              [enable the X11 gdk backend])],
			      [backend_set=yes])
AC_ARG_ENABLE(win32-backend,
              [AS_HELP_STRING([--enable-win32-backend],
                              [enable the Win32 gdk backend])],
			      [backend_set=yes])
AC_ARG_ENABLE(quartz-backend,
              [AS_HELP_STRING([--enable-quartz-backend],
                              [enable the quartz gdk backend])],
			      [backend_set=yes])
AC_ARG_ENABLE(broadway-backend,
              [AS_HELP_STRING([--enable-broadway-backend],
                              [enable the broadway (HTML5) gdk backend])],
			      [backend_set=yes])
AC_ARG_ENABLE(wayland-backend,
              [AS_HELP_STRING([--enable-wayland-backend],
                              [enable the wayland gdk backend])],
			      [backend_set=yes])

if test -z "$backend_set"; then
  if test "$platform_win32" = yes; then
    enable_win32_backend=yes
  else
    enable_x11_backend=yes
    enable_wayland_backend=maybe
  fi
fi

AC_ARG_ENABLE(quartz-relocation,
              [AS_HELP_STRING([--enable-quartz-relocation],
                              [enable bundle-based relocation functions])],
                              [quartz_relocation=yes])

cairo_backends=
backend_immodules=
have_gio_unix=no
GDK_BACKENDS=
GDK_EXTRA_LIBS=
GDK_EXTRA_CFLAGS=
GDK_WINDOWING=
PANGO_PACKAGES="pango pangocairo"

ENABLE_ON_X11='%'
if test "$enable_x11_backend" = "yes"; then
  # GDK calls the xlib backend "x11," cairo calls it "xlib." Other
  # backend names are identical.
  cairo_backends="$cairo_backends cairo-xlib"
  GDK_BACKENDS="$GDK_BACKENDS x11"
  # Pull in gio-unix for GDesktopAppInfo usage, see at least
  # gdkapplaunchcontext-x11.c
  have_gio_unix=yes
  backend_immodules="$backend_immodules,xim"
  GDK_WINDOWING="$GDK_WINDOWING
#define GDK_WINDOWING_X11"
  ENABLE_ON_X11=''
fi
AC_SUBST(ENABLE_ON_X11)

if test "$enable_win32_backend" = "yes"; then
  cairo_backends="$cairo_backends cairo-win32"
  GDK_BACKENDS="$GDK_BACKENDS win32"
  backend_immodules="$backend_immodules,ime"
  GDK_WINDOWING="$GDK_WINDOWING
#define GDK_WINDOWING_WIN32"
  GDK_EXTRA_LIBS="$GDK_EXTRA_LIBS -lgdi32 -limm32 -lshell32 -lole32 -Wl,-luuid -lwinmm"
  AM_CONDITIONAL(USE_WIN32, true)
  PANGO_PACKAGES="pangowin32 pangocairo"
else
  AM_CONDITIONAL(USE_WIN32, false)
fi

DISABLE_ON_QUARTZ=''
if test "x$enable_quartz_backend" = xyes; then
  cairo_backends="$cairo_backends cairo-quartz"
  GDK_BACKENDS="$GDK_BACKENDS quartz"
  backend_immodules="$backend_immodules,quartz"
  GDK_WINDOWING="$GDK_WINDOWING
#define GDK_WINDOWING_QUARTZ"
  GDK_EXTRA_LIBS="$GDK_EXTRA_LIBS -framework Cocoa"
  DISABLE_ON_QUARTZ='%'
  AM_CONDITIONAL(USE_QUARTZ, true)
  if test "x$quartz_relocation" = xyes; then
    AC_DEFINE([QUARTZ_RELOCATION], [1], [Use NSBundle functions to determine load paths for libraries, translations, etc.])
  fi
  # Pull in gio-unix for GDesktopAppInfo usage, see at least
  # gdkapplaunchcontext-x11.c
  have_gio_unix=yes

else
  AM_CONDITIONAL(USE_QUARTZ, false)
fi
AC_SUBST(DISABLE_ON_QUARTZ)

AC_CHECK_LIB([rt], [shm_open], [SHM_LIBS="-lrt"], [SHM_LIBS=""])
AC_SUBST(SHM_LIBS)

AC_CHECK_FUNCS(posix_fallocate)

if test "x$enable_broadway_backend" = xyes; then
  GDK_BACKENDS="$GDK_BACKENDS broadway"
  cairo_backends="$cairo_backends cairo"
  backend_immodules="$backend_immodules,broadway"
  if test "$os_win32" != "yes"; then
    have_gio_unix=yes
  fi
  GDK_WINDOWING="$GDK_WINDOWING
#define GDK_WINDOWING_BROADWAY"
  GDK_EXTRA_LIBS="$GDK_EXTRA_LIBS -lz"
  AM_CONDITIONAL(USE_BROADWAY, true)
else
  AM_CONDITIONAL(USE_BROADWAY, false)
fi

PKG_PROG_PKG_CONFIG

WAYLAND_DEPENDENCIES="wayland-client >= wayland_required_version xkbcommon >= 0.2.0 wayland-cursor >= wayland_required_version"
if test "$enable_wayland_backend" = "maybe" ; then
  AC_PATH_PROG([WAYLAND_SCANNER],[wayland-scanner],[no])
  PKG_CHECK_EXISTS($WAYLAND_DEPENDENCIES, [have_wayland_deps=yes], [have_wayland_deps=no])
  AC_MSG_CHECKING([for WAYLAND_DEPENDENCIES])
  if test "$WAYLAND_SCANNER" = "no" -o "$have_wayland_deps" = "no" ; then
    enable_wayland_backend=no
  else
    enable_wayland_backend=yes
  fi
  AC_MSG_RESULT($enable_wayland_backend)
fi

if test "$enable_wayland_backend" = "yes"; then
  # For the cairo image backend
  cairo_backends="$cairo_backends cairo"
  GDK_BACKENDS="$GDK_BACKENDS wayland"
  have_gio_unix=yes
  GDK_WINDOWING="$GDK_WINDOWING
#define GDK_WINDOWING_WAYLAND"
  WAYLAND_PACKAGES="$WAYLAND_DEPENDENCIES"
  AC_PATH_PROG([WAYLAND_SCANNER],[wayland-scanner],[no])
  AS_IF([test "x$WAYLAND_SCANNER" = "xno"],
    AC_MSG_ERROR([Could not find wayland-scanner in your PATH, required for parsing wayland extension protocols]))
  AC_SUBST([WAYLAND_SCANNER])

  AM_CONDITIONAL(USE_WAYLAND, true)
else
  AM_CONDITIONAL(USE_WAYLAND, false)
fi

# strip leading space
GDK_BACKENDS=${GDK_BACKENDS#* }

AC_SUBST(GDK_BACKENDS)

if test -z "$GDK_BACKENDS"; then
  AC_MSG_ERROR([No GDK backends selected.])
fi

if test "x$enable_debug" = "xyes"; then
  test "$cflags_set" = set || CFLAGS="$CFLAGS -g"
  GTK_DEBUG_FLAGS="-DG_ENABLE_DEBUG"
else
  if test "x$enable_debug" = "xno"; then
    GTK_DEBUG_FLAGS="-DG_DISABLE_ASSERT -DG_DISABLE_CHECKS -DG_DISABLE_CAST_CHECKS"
  else
    GTK_DEBUG_FLAGS="-DG_ENABLE_DEBUG -DG_DISABLE_CAST_CHECKS"
  fi
fi

# Build time sanity check...
AM_SANITY_CHECK

changequote(,)dnl
if test "x$GCC" = "xyes"; then
  case " $CFLAGS " in
  *[\ \	]-Wall[\ \	]*) ;;
  *) CFLAGS="$CFLAGS -Wall" ;;
  esac

  if test "x$enable_ansi" = "xyes"; then
    case " $CFLAGS " in
    *[\ \	]-ansi[\ \	]*) ;;
    *) CFLAGS="$CFLAGS -ansi" ;;
    esac

    case " $CFLAGS " in
    *[\ \	]-pedantic[\ \	]*) ;;
    *) CFLAGS="$CFLAGS -pedantic" ;;
    esac
  fi
fi
changequote([,])dnl

# Ensure MSVC-compatible struct packing convention is used when
# compiling for Win32 with gcc.
# What flag to depends on gcc version: gcc3 uses "-mms-bitfields", while
# gcc2 uses "-fnative-struct".
if test x"$os_win32" = xyes; then
  if test x"$GCC" = xyes; then
    msnative_struct=''
    AC_MSG_CHECKING([how to get MSVC-compatible struct packing])
    if test -z "$ac_cv_prog_CC"; then
      our_gcc="$CC"
    else
      our_gcc="$ac_cv_prog_CC"
    fi
    case `$our_gcc --version | sed -e 's,\..*,.,' -e q` in
      2.)
	if $our_gcc -v --help 2>/dev/null | grep fnative-struct >/dev/null; then
	  msnative_struct='-fnative-struct'
	fi
	;;
      *)
	if $our_gcc -v --help 2>/dev/null | grep ms-bitfields >/dev/null; then
	  msnative_struct='-mms-bitfields'
	fi
	;;
    esac
    if test x"$msnative_struct" = x ; then
      AC_MSG_RESULT([no way])
      AC_MSG_WARN([produced libraries might be incompatible with MSVC-compiled code])
    else
      CFLAGS="$CFLAGS $msnative_struct"
      AC_MSG_RESULT([${msnative_struct}])
    fi
  fi
fi

## Initial sanity check, done here so that users get told they
## have the wrong dependencies as early in the process as possible.
## Later on we actually use the cflags/libs from separate pkg-config
## calls. Oh, also the later pkg-config calls don't include
## the version requirements since those make the module lists
## annoying to construct
PKG_CHECK_MODULES(BASE_DEPENDENCIES,
  [glib-2.0 >= glib_required_version dnl
   atk >= atk_required_version dnl
   pango >= pango_required_version dnl
   cairo >= cairo_required_version dnl
   cairo-gobject >= cairo_required_version dnl
   gdk-pixbuf-2.0 >= gdk_pixbuf_required_version])

PKG_CHECK_MODULES(CAIRO_BACKEND, [$cairo_backends])

PKG_CHECK_MODULES(GMODULE, [gmodule-2.0])

dnl ******************************************************
dnl * See whether to include shared library dependencies *
dnl ******************************************************

AC_ARG_ENABLE(explicit-deps,
              [AS_HELP_STRING([--enable-explicit-deps=@<:@yes/no/auto@:>@],
                              [use explicit dependencies in .pc files [default=auto]])],,
              [enable_explicit_deps=auto])

AC_MSG_CHECKING([whether to write dependencies into .pc files])
case $enable_explicit_deps in
  auto)
    export SED
    deplibs_check_method=`(./libtool --config; echo 'eval echo \"$deplibs_check_method\"') | sh`
    if test "x$deplibs_check_method" != xpass_all || test "x$enable_static" = xyes ; then
      enable_explicit_deps=yes
    else
      enable_explicit_deps=no
    fi
  ;;
  yes|no)
  ;;
  *) AC_MSG_ERROR([Value given to --enable-explicit-deps must be one of yes, no or auto])
  ;;
esac
AC_MSG_RESULT($enable_explicit_deps)

AM_CONDITIONAL(DISABLE_EXPLICIT_DEPS, test $enable_explicit_deps = no)

# define a MAINT-like variable REBUILD which is set if Perl
# and awk are found, so autogenerated sources can be rebuilt

AC_PATH_PROGS(PERL, perl5 perl)

AC_CHECK_FUNCS(lstat mkstemp)
AC_CHECK_FUNCS(localtime_r)

# _NL_TIME_FIRST_WEEKDAY is an enum and not a define
AC_MSG_CHECKING([for _NL_TIME_FIRST_WEEKDAY])
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <langinfo.h>]],
                                [[char c;
                                  c = *((unsigned char *)  nl_langinfo(_NL_TIME_FIRST_WEEKDAY));]])],
               [gtk_ok=yes], [gtk_ok=no])
AC_MSG_RESULT($gtk_ok)
if test "$gtk_ok" = "yes"; then
  AC_DEFINE([HAVE__NL_TIME_FIRST_WEEKDAY], [1],
	    [Define if _NL_TIME_FIRST_WEEKDAY is available])
fi

# _NL_MEASUREMENT_MEASUREMENT is an enum and not a define
AC_MSG_CHECKING([for _NL_MEASUREMENT_MEASUREMENT])
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <langinfo.h>]],
                                [[char c;
                                  c = *((unsigned char *)  nl_langinfo(_NL_MEASUREMENT_MEASUREMENT));]])],
               [gtk_ok=yes], [gtk_ok=no])
AC_MSG_RESULT($gtk_ok)
if test "$gtk_ok" = "yes"; then
  AC_DEFINE([HAVE__NL_MEASUREMENT_MEASUREMENT], [1],
	    [Define if _NL_MEASUREMENT_MEASUREMENT is available])
fi

# _NL_PAPER_HEIGHT is an enum and not a define
AC_MSG_CHECKING([for _NL_PAPER_HEIGHT])
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <langinfo.h>]],
                                [[char c;
                                  c = *((unsigned char *)  nl_langinfo(_NL_PAPER_HEIGHT));]])],
               [gtk_ok=yes], [gtk_ok=no])
AC_MSG_RESULT($gtk_ok)
if test "$gtk_ok" = "yes"; then
  AC_DEFINE([HAVE__NL_PAPER_HEIGHT], [1],
	    [Define if _NL_PAPER_HEIGHT is available])
fi

# _NL_PAPER_WIDTH is an enum and not a define
AC_MSG_CHECKING([for _NL_PAPER_WIDTH])
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <langinfo.h>]],
                                [[char c;
                                  c = *((unsigned char *)  nl_langinfo(_NL_PAPER_WIDTH));]])],
               [gtk_ok=yes], [gtk_ok=no])
AC_MSG_RESULT($gtk_ok)
if test "$gtk_ok" = "yes"; then
  AC_DEFINE([HAVE__NL_PAPER_WIDTH], [1],
	    [Define if _NL_PAPER_WIDTH is available])
fi

# i18n stuff
ALL_LINGUAS="`grep -v '^#' "$srcdir/po/LINGUAS" | tr '\n' ' '`"
AM_GLIB_GNU_GETTEXT
LIBS="$LIBS $INTLLIBS"
AC_CONFIG_COMMANDS([po-properties],
                   [[case "$CONFIG_FILES" in *po-properties/Makefile.in*)
                       sed -e "/POTFILES =/r po-properties/POTFILES" po-properties/Makefile.in > po-properties/Makefile
                     esac]],
                   [[]])

dnl Snippet below is copied from AM_GLIB_GNU_GETTEXT to generate a first
dnl po-properties/POTFILES during configure; see GNOME #573515.
dnl
dnl Generate list of files to be processed by xgettext which will
dnl be included in po-properties/Makefile.
test -d po-properties || mkdir po-properties
if test "x$srcdir" != "x."; then
  if test "x`echo $srcdir | sed 's@/.*@@'`" = "x"; then
    popropsrcprefix="$srcdir/"
  else
    popropsrcprefix="../$srcdir/"
  fi
else
  popropsrcprefix="../"
fi
rm -f po-properties/POTFILES
sed -e "/^#/d" -e "/^\$/d" -e "s,.*,	$popropsrcprefix& \\\\," -e "\$s/\(.*\) \\\\/\1/" \
< $srcdir/po-properties/POTFILES.in > po-properties/POTFILES
dnl (End of adapted AM_GLIB_GNU_GETTEXT snippet.)

AM_GLIB_DEFINE_LOCALEDIR(GTK_LOCALEDIR)

dnl The DU4 header files don't provide library prototypes unless
dnl -std1 is given to the native cc.
AC_MSG_CHECKING([for extra flags to get ANSI library prototypes])

gtk_save_LIBS=$LIBS
LIBS="$LIBS -lm"
AC_RUN_IFELSE([AC_LANG_SOURCE([[#include <math.h>
                                int main (void) { return (log(1) != log(1.)); }]])],
              [AC_MSG_RESULT(none needed)],
              [gtk_save_CFLAGS="$CFLAGS"
               CFLAGS="$CFLAGS -std1"
               AC_RUN_IFELSE([AC_LANG_SOURCE([[#include <math.h>
                                               int main (void) { return (log(1) != log(1.)); }]])],
                             [AC_MSG_RESULT(-std1)],
                             [AC_MSG_RESULT()
                              CFLAGS="$gtk_save_CFLAGS"
                              AC_MSG_WARN([No ANSI prototypes found in library. (-std1 did not work.)])],
                             [true])],
              [AC_MSG_RESULT(none needed)])

LIBS=$gtk_save_LIBS

AC_MSG_CHECKING(for the BeOS)
case $host in
  *-*-beos*)
    AC_MSG_RESULT(yes)
    MATH_LIB=
  ;;
  *)
    AC_MSG_RESULT(no)
  ;;
esac

AC_SUBST(MATH_LIB)
#
# see bug 162979
#
AC_MSG_CHECKING(for HP-UX)
case $host_os in
  hpux9* | hpux10* | hpux11*)
    AC_MSG_RESULT(yes)
    CFLAGS="$CFLAGS -DHPPEX -DSHMLINK"
  ;;
  *)
    AC_MSG_RESULT(no)
  ;;
esac

#
# Run AM_PATH_GLIB_2_0 to make sure that GLib is installed and working
#

AM_PATH_GLIB_2_0(glib_required_version, :,
  AC_MSG_ERROR([
*** GLIB glib_required_version or better is required. The latest version of
*** GLIB is always available from ftp://ftp.gtk.org/pub/gtk/.]),
  gobject gmodule-no-export)

PKG_PROG_PKG_CONFIG_FOR_BUILD
GLIB_CFLAGS_FOR_BUILD=`$PKG_CONFIG_FOR_BUILD --cflags glib-2.0 gobject-2.0 gmodule-no-export-2.0`
GLIB_LIBS_FOR_BUILD=`$PKG_CONFIG_FOR_BUILD --libs glib-2.0 gobject-2.0 gmodule-no-export-2.0`
AC_SUBST(GLIB_CFLAGS_FOR_BUILD)
AC_SUBST(GLIB_LIBS_FOR_BUILD)

dnl Check for bind_textdomain_codeset, including -lintl if GLib brings it in.
dnl
gtk_save_LIBS=$LIBS
LIBS="$LIBS $GLIB_LIBS"
AC_CHECK_FUNCS(bind_textdomain_codeset)
LIBS=$gtk_save_LIBS

AC_CHECK_HEADERS(sys/mman.h,
                 AC_DEFINE(HAVE_SYS_MMAN_H, 1,
                           [Define to 1 if mman.h is available]))
AC_CHECK_HEADERS(sys/time.h,
                 AC_DEFINE(HAVE_SYS_TIME_H, 1,
                           [Define to 1 if time.h is available]))
AC_CHECK_HEADERS(unistd.h,
                 AC_DEFINE(HAVE_UNISTD_H, 1,
                           [Define to 1 if unistd.h is available]))
AC_CHECK_HEADERS(ftw.h,
                 AC_DEFINE(HAVE_FTW_H, 1,
                           [Define to 1 if ftw.h is available]))

AC_MSG_CHECKING([for GNU ftw extensions])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#define _XOPEN_SOURCE 500
                                     #define _GNU_SOURCE
                                     #include <ftw.h>]],
                                   [[int flags = FTW_ACTIONRETVAL;]])],
                  [gtk_ok=yes],[gtk_ok=no])
if test $gtk_ok = yes; then
    AC_MSG_RESULT([yes])
    AC_DEFINE(HAVE_GNU_FTW, 1, [Have GNU ftw])
else
    AC_MSG_RESULT([no])
fi

#
# Disable deprecation checks for all libraries we depend on on stable branches.
# This is so newer versions of those libraries don't cause more warnings with
# a stable GTK version.
# We don't ever want to turn off deprecation warnings for master however, because
# that's where we get rid of deprecated API we use.
#
if test m4_eval(gtk_minor_version % 2) = 0 ; then
  AC_DEFINE_UNQUOTED(GLIB_DISABLE_DEPRECATION_WARNINGS, 1,
                     [Disable deprecation warnings from glib])
fi

dnl
saved_cflags="$CFLAGS"
saved_ldflags="$LDFLAGS"


# Checks for library functions.
AC_FUNC_MMAP

AC_CHECK_FUNCS(mallinfo)
AC_CHECK_FUNCS(getresuid)
AC_TYPE_UID_T

# Check for round(), rint(), isnan(), isinf() and nearbyint()
AC_CHECK_LIB(m,round,,)
AC_CHECK_FUNCS(round rint nearbyint)
AC_CHECK_DECLS([isnan, isinf], [], [], [[#include <math.h>]])

# Checks for gdkspawn
AC_CHECK_HEADERS(crt_externs.h)
AC_CHECK_FUNCS(_NSGetEnviron)

AC_MSG_CHECKING(whether to build dynamic modules)

AC_ARG_ENABLE(modules,
              [AS_HELP_STRING([--disable-modules],
                              [disable dynamic module loading])])

dynworks=false
build_dynamic_modules=no
deps=
if test x$enable_modules = xno; then
    AC_MSG_RESULT(no)
else
    AC_MSG_RESULT(yes)
    AC_MSG_CHECKING(whether dynamic modules work)
    ## for loop is to strip newline
    tmp=`$PKG_CONFIG --variable=gmodule_supported gmodule-no-export-2.0`
    for I in $tmp; do
        dynworks=$I
    done

    dnl Now we check to see if our libtool supports shared lib deps
    dnl (in a rather ugly way even)
    if $dynworks; then
        module_libtool_config="${CONFIG_SHELL-/bin/sh} ./libtool --config"
        module_deplibs_check=`$module_libtool_config | \
            grep '^[[a-z_]]*check[[a-z_]]*_method=[['\''"]]' | \
            sed 's/.*[['\''"]]\(.*\)[['\''"]]$/\1/'`
        if test "x$module_deplibs_check" = "xnone" || \
           test "x$module_deplibs_check" = "xunknown" || \
           test "x$module_deplibs_check" = "x"; then
            dynworks=false
        fi
    fi

    if $dynworks; then
        build_dynamic_modules=yes
        AC_DEFINE(USE_GMODULE, 1,
                  [Define to 1 if gmodule works and should be used])
        AC_MSG_RESULT(yes)
    else
        build_dynamic_modules=no
        AC_MSG_RESULT(no)
    fi
fi

AM_CONDITIONAL(BUILD_DYNAMIC_MODULES, $dynworks)

#
# Allow building some or all immodules included
#
AC_MSG_CHECKING(immodules to build)

AC_ARG_WITH(included_immodules,
            [AS_HELP_STRING([--with-included-immodules=MODULE1,MODULE2,...],
                            [build the specified input methods into gtk])])

if $dynworks; then
   :
else
   ## if the option was specified, leave it; otherwise disable included immodules
   if test x$with_included_immodules = xno; then
           with_included_immodules=yes
   fi
fi

all_immodules="am-et,cedilla,cyrillic-translit,inuktitut,ipa,multipress,thai,ti-er,ti-et,viqr$backend_immodules"

included_immodules=""
# If the switch specified without listing any specific ones, include all
if test "x$with_included_immodules" = xyes ; then
  included_immodules="$all_immodules"
else
  included_immodules="$with_included_immodules"
fi

AC_MSG_RESULT($included_immodules)
AM_CONDITIONAL(HAVE_INCLUDED_IMMMODULES, test "x$included_immodules" != x)

INCLUDED_IMMODULE_OBJ=
INCLUDED_IMMODULE_DEFINE=

IFS="${IFS= 	}"; gtk_save_ifs="$IFS"; IFS=","
for immodule in $included_immodules; do
 immodule_underscores=`echo $immodule | sed -e 's/-/_/g'`
 if echo "$all_immodules" | egrep "(^|,)$immodule(\$|,)" > /dev/null; then
   :
 else
   AC_MSG_ERROR([the specified input method $immodule does not exist])
 fi

 INCLUDED_IMMODULE_OBJ="$INCLUDED_IMMODULE_OBJ ../modules/input/libstatic-im-$immodule.la"
 INCLUDED_IMMODULE_DEFINE="$INCLUDED_IMMODULE_DEFINE -DINCLUDE_IM_$immodule_underscores"
 eval INCLUDE_$immodule_underscores=yes
done
IFS="$gtk_save_ifs"
AC_SUBST(INCLUDED_IMMODULE_OBJ)
AC_SUBST(INCLUDED_IMMODULE_DEFINE)

AM_CONDITIONAL(INCLUDE_IM_AM_ET, [test x"$INCLUDE_am_et" = xyes])
AM_CONDITIONAL(INCLUDE_IM_CEDILLA, [test x"$INCLUDE_cedilla" = xyes])
AM_CONDITIONAL(INCLUDE_IM_CYRILLIC_TRANSLIT, [test x"$INCLUDE_cyrillic_translit" = xyes])
AM_CONDITIONAL(INCLUDE_IM_IME, [test x"$INCLUDE_ime" = xyes])
AM_CONDITIONAL(INCLUDE_IM_INUKTITUT, [test x"$INCLUDE_inuktitut" = xyes])
AM_CONDITIONAL(INCLUDE_IM_IPA, [test x"$INCLUDE_ipa" = xyes])
AM_CONDITIONAL(INCLUDE_IM_MULTIPRESS, [test x"$INCLUDE_multipress" = xyes])
AM_CONDITIONAL(INCLUDE_IM_QUARTZ, [test x"$INCLUDE_quartz" = xyes])
AM_CONDITIONAL(INCLUDE_IM_BROADWAY, [test x"$INCLUDE_broadway" = xyes])
AM_CONDITIONAL(INCLUDE_IM_THAI, [test x"$INCLUDE_thai" = xyes])
AM_CONDITIONAL(INCLUDE_IM_TI_ER, [test x"$INCLUDE_ti_er" = xyes])
AM_CONDITIONAL(INCLUDE_IM_TI_ET, [test x"$INCLUDE_ti_et" = xyes])
AM_CONDITIONAL(INCLUDE_IM_VIQR, [test x"$INCLUDE_viqr" = xyes])
AM_CONDITIONAL(INCLUDE_IM_XIM, [test x"$INCLUDE_xim" = xyes])

# Checks to see whether we should include mediaLib
# support.
#
AC_CHECK_HEADER(sys/systeminfo.h,
                AC_DEFINE(HAVE_SYS_SYSTEMINFO_H, 1,
                          [Define to 1 if sys/systeminfo.h is available]))
AC_CHECK_HEADER(sys/sysinfo.h,
                AC_DEFINE(HAVE_SYS_SYSINFO_H, 1,
                          [Define to 1 if sys/sysinfo.h is available]))

dnl Look for a host system's gdk-pixbuf-csource if we are cross-compiling

AM_CONDITIONAL(CROSS_COMPILING, test $cross_compiling = yes)

AS_IF([test "x$enable_gtk2_dependency" = xyes],
      [AC_PATH_PROG(GTK_UPDATE_ICON_CACHE, gtk-update-icon-cache, no)
       if test x$GTK_UPDATE_ICON_CACHE = xno; then
         AC_MSG_ERROR([
*** --enable-gtk2-dependency has been set but no existing
*** `gtk-update-icon-cache` has been found in the path.])
       fi],

      [test "x$cross_compiling" = xyes],
      [# If no GTK+2 dependency and cross compiling, we need to find a host gdk-pixbuf.
       # pkg.m4 blocks all variable starting with PKG, so allow this one
       m4_pattern_allow([PKG_CONFIG_FOR_BUILD])

       AS_IF([test x$PKG_CONFIG_FOR_BUILD = x],
             [AC_MSG_ERROR([You must define PKG_CONFIG_FOR_BUILD when cross compiling])])

       AC_MSG_CHECKING([for native gdk-pixbuf])
       AS_IF([AC_RUN_LOG([$PKG_CONFIG_FOR_BUILD --exists --print-errors gdk-pixbuf-2.0])],
             [AC_MSG_RESULT([yes])],
             [AC_MSG_ERROR([native gdk-pixbuf not found])])

       NATIVE_GDKPIXBUF_CFLAGS=`$PKG_CONFIG_FOR_BUILD --cflags gdk-pixbuf-2.0`
       NATIVE_GDKPIXBUF_LIBS=`$PKG_CONFIG_FOR_BUILD --libs gdk-pixbuf-2.0`
       # This is the native gtk-update-icon-cache which will be used at
       # build time, NOT the one installed on the target host.
       GTK_UPDATE_ICON_CACHE="`cd $builddir && pwd`/gtk/native/native-update-icon-cache$BUILD_EXEEXT"
       AC_SUBST(NATIVE_GDKPIXBUF_CFLAGS)
       AC_SUBST(NATIVE_GDKPIXBUF_LIBS)
       AC_SUBST(GTK_UPDATE_ICON_CACHE)],

      [# In native compiling case, the gtk-update-icon-cache installed
       # is the same as the one used during build.
       GTK_UPDATE_ICON_CACHE="\$(abs_top_builddir)/gtk/gtk-update-icon-cache$EXEEXT"
       AC_SUBST(GTK_UPDATE_ICON_CACHE)]
)

AM_CONDITIONAL(USE_EXTERNAL_ICON_CACHE, [test "x$enable_gtk2_dependency" = xyes])

########################################
# Windowing system checks
########################################

# GTK+ uses some X calls, so needs to link against X directly
GTK_DEP_PACKAGES_FOR_X=
GTK_DEP_LIBS_FOR_X=
X_EXTENSIONS=

if test "x$enable_x11_backend" = xyes; then
  X_PACKAGES=fontconfig

  #
  # We use fontconfig very peripherally when decoding the default
  # settings.
  #
  if $PKG_CONFIG --exists fontconfig; then : ; else
    AC_MSG_ERROR([
*** fontconfig (http://www.fontconfig.org) is required by the X11 backend.])
  fi

  #
  # Check for basic X packages; we use pkg-config if available
  #
  if $PKG_CONFIG --exists x11 xext; then
    have_base_x_pc=true
    X_PACKAGES="$X_PACKAGES x11 xext"
    x_libs="`$PKG_CONFIG --libs x11 xext`"
    X_CFLAGS="`$PKG_CONFIG --cflags x11 xext`"

    # Strip out any .la files that pkg-config might give us (this happens
    # with -uninstalled.pc files)
    x_libs_for_checks=
    for I in $x_libs ; do
      case $I in
        *.la) ;;
        *) x_libs_for_checks="$x_libs_for_checks $I" ;;
      esac
    done

    GTK_PACKAGES_FOR_X="x11"
  else
    have_base_x_pc=false
    AC_PATH_XTRA
    if test x$no_x = xyes ; then
      AC_MSG_ERROR([X development libraries not found])
    fi

    x_cflags="$X_CFLAGS"
    x_libs_for_checks="$X_LIBS -lXext -lX11 $X_EXTRA_LIBS"

    GTK_DEP_LIBS_FOR_X="$X_LIBS -lX11 $X_EXTRA_LIBS"
  fi

  # Extra libraries found during checks (-lXinerama, etc), not from pkg-config.
  x_extra_libs=

  gtk_save_cppflags="$CPPFLAGS"
  CPPFLAGS="$CPPFLAGS $X_CFLAGS"

  gtk_save_LIBS=$LIBS
  LIBS="$x_libs_for_checks $LIBS"

  # Sanity check for the X11 and Xext libraries. While everything we need from
  # Xext is optional, the chances a system has *none* of these things is so
  # small that we just unconditionally require it.
  AC_CHECK_FUNC(XOpenDisplay, :,
                AC_MSG_ERROR([*** libX11 and libXext not found. Check 'config.log' for more details.]))
  AC_CHECK_FUNC(XextFindDisplay, :,
                AC_MSG_ERROR([*** libX11 and libXext not found. Check 'config.log' for more details.]))

  # Check for xReply

  AC_MSG_CHECKING([if <X11/extensions/XIproto.h> is needed for xReply])
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <X11/Xlibint.h>]],
                                     [[xReply *rep = NULL;
                                       rep = rep;]])],
                    [AC_MSG_RESULT([no])],
                    [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <X11/extensions/XIproto.h>
                                                          #include <X11/Xlibint.h>]],
                                                        [[xReply *rep = NULL;
                                                          rep = rep;]])],
                                       [AC_MSG_RESULT([yes])
                                        AC_DEFINE([NEED_XIPROTO_H_FOR_XREPLY], [1],
                                                  [Define if <X11/extensions/XIproto.h> needed for xReply])],
                                       [AC_MSG_RESULT([unknown])
                                        AC_MSG_ERROR([xReply type unavailable. X11 is too old])])])

  # Check for XKB support.

  if test "x$enable_xkb" = "xyes"; then
        AC_CHECK_FUNC(XkbQueryExtension,
                      X_EXTENSIONS="$X_EXTENSIONS XKB"
                      AC_DEFINE(HAVE_XKB, 1, [Define to use XKB extension]),
                      AC_MSG_ERROR([*** XKB extension not found. Check 'config.log' for more details.]))
  elif test "x$enable_xkb" = "xmaybe"; then
        AC_CHECK_FUNC(XkbQueryExtension,
                      X_EXTENSIONS="$X_EXTENSIONS XKB"
                      AC_DEFINE(HAVE_XKB, 1, [Define to use XKB extension]))
  else
        AC_MSG_WARN(XKB support explicitly disabled)
  fi

  # Check for shaped window extension

  AC_CHECK_FUNC(XShapeCombineMask, :,
     [AC_MSG_ERROR([Shape extension not found, check your development headers])])

  # X SYNC check
  gtk_save_CFLAGS="$CFLAGS"
  CFLAGS="$CFLAGS $x_cflags"

  AC_CHECK_FUNC(XSyncQueryExtension,
      [AC_CHECK_HEADER(X11/extensions/sync.h,
	  AC_DEFINE(HAVE_XSYNC, 1, [Have the SYNC extension library]),
	  :, [#include <X11/Xlib.h>])])

  CFLAGS="$gtk_save_CFLAGS"

  if test "x$enable_xinerama" != "xno"; then
    # Check for Xinerama extension (Solaris impl or Xfree impl)
    have_xfree_xinerama=false
    have_solaris_xinerama=false

    gtk_save_cppflags="$CPPFLAGS"
    CPPFLAGS="$CPPFLAGS $x_cflags"

    # Check for XFree
    AC_MSG_CHECKING(for Xinerama packages)

    if $PKG_CONFIG --exists xinerama ; then
       AC_MSG_RESULT(yes)
       have_xfree_xinerama=true
       X_PACKAGES="$X_PACKAGES xinerama"
    else
       AC_MSG_RESULT(no)
       AC_CHECK_LIB(Xinerama, XineramaQueryExtension,
	   [AC_CHECK_HEADER(X11/extensions/Xinerama.h,
	   [GTK_ADD_LIB(x_extra_libs,Xinerama)
	   have_xfree_xinerama=true], :,
           [#include <X11/Xlib.h>])])
    fi

    if $have_xfree_xinerama ; then
      X_EXTENSIONS="$X_EXTENSIONS Xinerama"
      AC_DEFINE(HAVE_XFREE_XINERAMA, 1,
                [Define to 1 if XFree Xinerama is available])
      AC_DEFINE(HAVE_XINERAMA, 1,
                [Define to 1 is Xinerama is available])
    else
      case "$host" in
        *-*-solaris*)
            # Check for solaris

	    have_solaris_xinerama=false
	    AC_CHECK_FUNC(XineramaGetInfo,
	        [AC_CHECK_HEADER(X11/extensions/xinerama.h,
		    [have_solaris_xinerama=true], :,
		    [#include <X11/Xlib.h>])])

	    AC_MSG_CHECKING(for Xinerama support on Solaris)

            if $have_solaris_xinerama ; then
              X_EXTENSIONS="$X_EXTENSIONS Xinerama"
              AC_DEFINE(HAVE_SOLARIS_XINERAMA, 1,
                        [Define to 1 if solaris xinerama is available])
	      AC_DEFINE(HAVE_XINERAMA, 1,
                        [Define to 1 if xinerama is available])
              AC_MSG_RESULT(yes)
            else
              AC_MSG_RESULT(no)
            fi
            ;;
        *)
            ;;
      esac
    fi
  fi
  if test "x$enable_xinerama" = "xyes" ; then
    if test "x$have_xfree_xinerama" != "xtrue" -a "x$have_solaris_xinerama" != "xtrue" ; then
      AC_MSG_ERROR([*** Xinerama extension not found. Check 'config.log' for more details.])
    fi
  fi

  # Check for XGetEventData for GenericEvents
  AC_CHECK_FUNC(XGetEventData,
                AC_DEFINE(HAVE_XGENERICEVENTS, 1, [Have XGenericEvent]))

  # set up things for XInput
  if $PKG_CONFIG --exists "xi" ; then

    X_PACKAGES="$X_PACKAGES xi"
    GTK_PACKAGES_FOR_X="$GTK_PACKAGES_FOR_X xi"

    AC_CHECK_HEADER(X11/extensions/XInput2.h,
                    have_xinput2=yes
                    AC_DEFINE(XINPUT_2, 1, [Define to 1 if XInput 2.0 is available]))

    gtk_save_LIBS="$LIBS"
    LIBS="$LIBS -lXi"

    # Note that we also check that the XIScrollClassInfo struct is defined,
    # because at least Ubuntu Oneiric seems to have XIAllowTouchEvents(), but not the XIScrollClassInfo struct.
    AC_CHECK_FUNC([XIAllowTouchEvents],
      [AC_CHECK_MEMBER([XIScrollClassInfo.number],
                       have_xinput2_2=yes
                       AC_DEFINE(XINPUT_2_2, 1, [Define to 1 if XInput 2.2 is available]),
                       have_xinput2_2=no,
                       [[#include <X11/extensions/XInput2.h>]])])
    LIBS="$gtk_save_LIBS"

    if test "x$have_xinput2_2" = "xyes"; then
      X_EXTENSIONS="$X_EXTENSIONS XI2.2"
    else
      X_EXTENSIONS="$X_EXTENSIONS XI2"
    fi
  fi

  AS_IF([test "x$have_xinput2" != "xyes"],
        [AC_MSG_ERROR([*** XInput2 extension not found. Check 'config.log' for more details.])])

  # Check for the RANDR extension
  if test x"$enable_xrandr" != xno; then
    if $PKG_CONFIG --exists "xrandr >= 1.2.99" ; then
      AC_DEFINE(HAVE_RANDR, 1, [Have the Xrandr extension library])

      X_PACKAGES="$X_PACKAGES xrandr"
      X_EXTENSIONS="$X_EXTENSIONS XRANDR"
    elif test x"$enable_xrandr" = xyes; then
      AC_MSG_ERROR([RANDR support requested but xrandr not found])
    fi
  fi

  # Checks for Xcursor library

  if $PKG_CONFIG --exists xcursor ; then
    AC_DEFINE(HAVE_XCURSOR, 1, [Have the Xcursor library])

    X_PACKAGES="$X_PACKAGES xcursor"
  fi

  # Checks for XFixes extension

  if test x"$enable_xfixes" != xno; then
    if $PKG_CONFIG --exists xfixes ; then
      AC_DEFINE(HAVE_XFIXES, 1, [Have the XFIXES X extension])

      X_PACKAGES="$X_PACKAGES xfixes"
      X_EXTENSIONS="$X_EXTENSIONS XFIXES"
      GTK_PACKAGES_FOR_X="$GTK_PACKAGES_FOR_X xfixes"
    elif test x"$enable_xfixes" = xyes; then
      AC_MSG_ERROR([XFixes support requested but xfixes not found])
    fi
  fi

  # Checks for Xcomposite extension

  if test x"$enable_xcomposite" != xno; then
    if $PKG_CONFIG --exists xcomposite ; then
      AC_DEFINE(HAVE_XCOMPOSITE, 1, [Have the XCOMPOSITE X extension])

      X_PACKAGES="$X_PACKAGES xcomposite"
      X_EXTENSIONS="$X_EXTENSIONS Composite"
      GTK_PACKAGES_FOR_X="$GTK_PACKAGES_FOR_X xcomposite"
    elif test x"$enable_xcomposite" = xyes; then
      AC_MSG_ERROR([Xcomposite support requested but xcomposite not found])
    fi
  fi

  # Checks for Xdamage extension

  if test x"$enable_xdamage" != xno; then
    if $PKG_CONFIG --exists xdamage ; then
      AC_DEFINE(HAVE_XDAMAGE, 1, [Have the XDAMAGE X extension])

      X_PACKAGES="$X_PACKAGES xdamage"
      X_EXTENSIONS="$X_EXTENSIONS DAMAGE"
      GTK_PACKAGES_FOR_X="$GTK_PACKAGES_FOR_X xdamage"
    elif test x"$enable_xdamage" = xyes; then
      AC_MSG_ERROR([Xdamage support requested but xdamage not found])
    fi
  fi

  if $have_base_x_pc ; then
    GDK_EXTRA_LIBS="$x_extra_libs"
  else
    GDK_EXTRA_LIBS="$X_LIBS $x_extra_libs -lXext -lX11 $GDK_EXTRA_LIBS"
  fi

  CPPFLAGS="$gtk_save_cppflags"
  LIBS="$gtk_save_libs"

  AM_CONDITIONAL(USE_X11, true)

  # strip leading space
  X_EXTENSIONS=${X_EXTENSIONS#* }

else
  XPACKAGES=

  AM_CONDITIONAL(USE_X11, false)
fi

# Check for gio-unix
if test "$have_gio_unix" = "yes"; then
  GDK_GIO_PACKAGE="gio-unix-2.0 >= glib_required_version"
  AC_DEFINE([HAVE_GIO_UNIX], [1],
            [Define if gio-unix is available])
else
  GDK_GIO_PACKAGE="gio-2.0 >= glib_required_version"
fi

# Check for Pango flags

AC_MSG_CHECKING(Pango flags)
if $PKG_CONFIG --exists $PANGO_PACKAGES ; then
        PANGO_CFLAGS=`$PKG_CONFIG --cflags $PANGO_PACKAGES`
        PANGO_LIBS=`$PKG_CONFIG --libs $PANGO_PACKAGES`

        AC_MSG_RESULT($PANGO_CFLAGS $PANGO_LIBS)
else
        AC_MSG_ERROR([
*** Pango not found. Pango built with Cairo support is required
*** to build GTK+. See http://www.pango.org for Pango information.
])
fi

CFLAGS="$CFLAGS $PANGO_CFLAGS"

if $PKG_CONFIG --uninstalled $PANGO_PACKAGES; then
        :
else
	gtk_save_LIBS="$LIBS"
        LIBS="$PANGO_LIBS $LIBS"
        AC_TRY_LINK_FUNC(pango_context_new, :, AC_MSG_ERROR([
*** Can't link to Pango. Pango is required to build
*** GTK+. For more information see http://www.pango.org]))
        LIBS="$gtk_save_LIBS"
fi

# Check for cairo_set_device_scale, as we don't want to depend hard on
# this until there is a stable release with it
CAIRO_CFLAGS=`$PKG_CONFIG --cflags cairo`
CAIRO_LIBS=`$PKG_CONFIG --libs cairo`
CFLAGS="$CFLAGS $CAIRO_CFLAGS"
gtk_save_LIBS="$LIBS"
LIBS="$CAIRO_LIBS $LIBS"
AC_CHECK_FUNCS(cairo_surface_set_device_scale)
LIBS="$gtk_save_LIBS"


CFLAGS="$saved_cflags"
LDFLAGS="$saved_ldflags"

GDK_PACKAGES="$PANGO_PACKAGES gdk-pixbuf-2.0 >= gdk_pixbuf_required_version cairo >= cairo_required_version cairo-gobject >= cairo_required_version"
GDK_PRIVATE_PACKAGES="$GDK_GIO_PACKAGE $X_PACKAGES $WAYLAND_PACKAGES $cairo_backends"
if test "x$enable_x11_backend" = xyes; then
  GDK_PRIVATE_PACKAGES="$GDK_PRIVATE_PACKAGES pangoft2"
fi

PKG_CHECK_MODULES(GDK_DEP, $GDK_PACKAGES $GDK_PRIVATE_PACKAGES)
GDK_DEP_LIBS="$GDK_EXTRA_LIBS $GDK_DEP_LIBS $MATH_LIB"
GDK_DEP_CFLAGS="$GDK_DEP_CFLAGS $GDK_EXTRA_CFLAGS"
#
# If we aren't writing explicit dependencies, then don't put the extra libraries we need
# into the pkg-config files
#
if test $enable_explicit_deps != yes ; then
  GDK_EXTRA_LIBS=
fi

AC_SUBST(GDK_PACKAGES)
AC_SUBST(GDK_PRIVATE_PACKAGES)
AC_SUBST(GDK_EXTRA_LIBS)
AC_SUBST(GDK_EXTRA_CFLAGS)
AC_SUBST(GDK_DEP_LIBS)
AC_SUBST(GDK_DEP_CFLAGS)


########################################
# Check for Accessibility Toolkit flags
########################################

if test x$enable_x11_backend = xyes; then
   ATK_PACKAGES="atk atk-bridge-2.0"
else
   ATK_PACKAGES="atk"
fi

PKG_CHECK_MODULES(ATK, $ATK_PACKAGES)

GTK_PACKAGES="atk >= atk_required_version cairo >= cairo_required_version cairo-gobject >= cairo_required_version gdk-pixbuf-2.0 >= gdk_pixbuf_required_version gio-2.0 >= glib_required_version"
GTK_PRIVATE_PACKAGES="$ATK_PACKAGES"
if test "x$enable_x11_backend" = xyes; then
  GTK_PRIVATE_PACKAGES="$GTK_PRIVATE_PACKAGES pangoft2"
fi
if test "$have_gio_unix" = "yes"; then
  GTK_PRIVATE_PACKAGES="$GTK_PRIVATE_PACKAGES gio-unix-2.0 >= glib_required_version"
fi
GTK_EXTRA_LIBS=

GTK_EXTRA_CFLAGS=
PKG_CHECK_MODULES(GTK_DEP, $PANGO_PACKAGES $GTK_PACKAGES_FOR_X $GTK_PACKAGES $GTK_PRIVATE_PACKAGES)
GTK_DEP_LIBS="$GDK_EXTRA_LIBS $GTK_DEP_LIBS_FOR_X $GTK_DEP_LIBS $GTK_EXTRA_LIBS $MATH_LIB"
GTK_DEP_CFLAGS="$GTK_DEP_CFLAGS $GDK_EXTRA_CFLAGS $GTK_EXTRA_CFLAGS"

if test x"$os_win32" = xyes; then
  GTK_EXTRA_CFLAGS="$msnative_struct"
fi

GLIB_PREFIX="`$PKG_CONFIG --variable=prefix glib-2.0`"
ATK_PREFIX="`$PKG_CONFIG --variable=prefix atk`"
PANGO_PREFIX="`$PKG_CONFIG --variable=prefix pango`"
CAIRO_PREFIX="`$PKG_CONFIG --variable=prefix cairo`"

AC_SUBST(GTK_PACKAGES)
AC_SUBST(GTK_PRIVATE_PACKAGES)
AC_SUBST(GTK_EXTRA_LIBS)
AC_SUBST(GTK_EXTRA_CFLAGS)
AC_SUBST(GTK_DEP_LIBS)
AC_SUBST(GTK_DEP_CFLAGS)

AC_SUBST(GLIB_PREFIX)
AC_SUBST(ATK_PREFIX)
AC_SUBST(PANGO_PREFIX)
AC_SUBST(CAIRO_PREFIX)

AC_SUBST(GTK_DEBUG_FLAGS)
AC_SUBST(GTK_XIM_FLAGS)

GDK_PIXBUF_LIBS=`$PKG_CONFIG --libs gdk-pixbuf-2.0`
AC_SUBST(GDK_PIXBUF_LIBS)

########################
# Checks needed for gail
########################

old_LIBS="$LIBS"
dnl Checks for inet libraries:
AC_SEARCH_LIBS(gethostent, nsl)
AC_SEARCH_LIBS(setsockopt, socket)
AC_SEARCH_LIBS(connect, inet)

dnl check for the sockaddr_un.sun_len member
AC_CHECK_MEMBER([struct sockaddr_un.sun_len],
		[struct_sockaddr_un_sun_len=true],
		[struct_sockaddr_un_suin_len=false],
		[#include <sys/types.h>
		 #include <sys/un.h>]
		)
case $struct_sockaddr_un_sun_len in
	true)
		AC_DEFINE_UNQUOTED(HAVE_SOCKADDR_UN_SUN_LEN, 1,
		                   [Have the sockaddr_un.sun_len member])
		;;
	*)
		;;
esac

GAIL_INET_LIBS="$LIBS"
AC_SUBST([GAIL_INET_LIBS])

LIBS="$old_LIBS"

################################################################
# Printing system checks
################################################################

PRINT_BACKENDS="file lpr"

AC_ARG_ENABLE(cups,
              [AS_HELP_STRING([--disable-cups],
                              [disable cups print backend])],,
              [enable_cups=auto])

if test "x$enable_cups" = "xno"; then
  AM_CONDITIONAL(HAVE_CUPS, false)
else
  AC_PATH_TOOL(CUPS_CONFIG, cups-config, no)
  if test "x$CUPS_CONFIG" = "xno"; then
    if test "x$enable_cups" = "xauto"; then
      AM_CONDITIONAL(HAVE_CUPS, false)
    else
      AC_MSG_ERROR([
*** cups not found.
])
    fi
  else
    CUPS_CFLAGS=`$CUPS_CONFIG --cflags | sed 's/-O[0-9]*//' | sed 's/-m[^\t]*//g'`
    CUPS_LIBS=`$CUPS_CONFIG --libs`

    CUPS_API_VERSION=`$CUPS_CONFIG --api-version`
    CUPS_API_MAJOR=`echo $ECHO_N $CUPS_API_VERSION | awk -F. '{print $1}'`
    CUPS_API_MINOR=`echo $ECHO_N $CUPS_API_VERSION | awk -F. '{print $2}'`

    if test $CUPS_API_MAJOR -gt 1 -o \
            $CUPS_API_MAJOR -eq 1 -a $CUPS_API_MINOR -lt 2; then
      AC_MSG_ERROR([CUPS >= 1.2 not found])
    fi
    if test $CUPS_API_MAJOR -gt 1 -o \
	    $CUPS_API_MAJOR -eq 1 -a $CUPS_API_MINOR -ge 6; then
      AC_DEFINE(HAVE_CUPS_API_1_6, 1,
                [Define to 1 if CUPS 1.6 API is available])
      have_cups_api_1_6=yes
    fi

    AC_SUBST(CUPS_API_MAJOR)
    AC_SUBST(CUPS_API_MINOR)
    AC_SUBST(CUPS_CFLAGS)
    AC_SUBST(CUPS_LIBS)

    AC_CHECK_HEADER(cups/cups.h,,AC_MSG_ERROR([[*** Sorry, cups-config present but cups/cups.h missing.]]))

    PRINT_BACKENDS="$PRINT_BACKENDS cups"
    AM_CONDITIONAL(HAVE_CUPS, true)

    gtk_save_cflags="$CFLAGS"
    CFLAGS="$CUPS_CFLAGS"
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <cups/http.h>]],
                                       [[http_t http; char *s = http.authstring;]])],
                      [AC_DEFINE([HAVE_HTTP_AUTHSTRING], [],
                                 [Define if cups http_t authstring field is accessible])],
                      [])
    CFLAGS="$gtk_save_cflags"

    AC_SUBST(HAVE_HTTP_AUTHSTRING)

    gtk_save_libs="$LIBS"
    LIBS="$CUPS_LIBS"
    AC_CHECK_FUNCS(httpGetAuthString)
    LIBS="$gtk_save_libs"
  fi
fi

# Checks to see if we should compile with PAPI backend for GTK+
#

AC_ARG_ENABLE(papi,
              [AS_HELP_STRING([--disable-papi],
                              [disable papi print backend])],,
              [enable_papi=auto])

if test "x$enable_papi" = "xno"; then
  AM_CONDITIONAL(HAVE_PAPI, false)
else
  AC_MSG_CHECKING(libpapi)
  AC_CHECK_LIB(papi, papiServiceCreate, have_papi=yes, have_papi=no)
  if test $have_papi = yes; then
    PRINT_BACKENDS="$PRINT_BACKENDS papi"
    AC_DEFINE([HAVE_PAPI], [], [Define to 1 if libpapi available])
  fi
  AM_CONDITIONAL(HAVE_PAPI, test $have_papi = yes)
  if test "x$enable_papi" = "xyes" -a "x$have_papi" = "xno"; then
    AC_MSG_ERROR([
*** papi not found.
])
  fi
fi

# Checks to see if we should compile with cloudprint backend for GTK+
#

AC_ARG_ENABLE(cloudprint,
              [AS_HELP_STRING([--disable-cloudprint],
                              [disable cloudprint print backend])],,
              [enable_cloudprint=auto])

if test "x$enable_cloudprint" = "xno"; then
  AM_CONDITIONAL(HAVE_CLOUDPRINT, false)
else
  PKG_CHECK_MODULES(REST, [rest-0.7], have_rest=yes, have_rest=no)
  PKG_CHECK_MODULES(JSON_GLIB, [json-glib-1.0], have_json_glib=yes, have_json_glib=no)
  if test "x$have_rest" = "xyes" -a "x$have_json_glib" = "xyes"; then
    PRINT_BACKENDS="$PRINT_BACKENDS cloudprint"
  fi
  AM_CONDITIONAL(HAVE_CLOUDPRINT, test "x$have_rest" = "xyes" -a "x$have_json_glib" = "xyes")
fi

if test "x$enable_cloudprint" = "xyes" -a "x$have_rest" = "xno"; then
    AC_MSG_ERROR([
*** rest not found.
])
fi

if test "x$enable_cloudprint" = "xyes" -a "x$have_json_glib" = "xno"; then
    AC_MSG_ERROR([
*** json-glib not found.
])
fi

AM_CONDITIONAL(HAVE_PAPI_CUPS, test "x$have_papi" = "xyes" -a "x$CUPS_CONFIG" != "xno")

gtk_save_cppflags="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS $GTK_DEP_CFLAGS $GDK_DEP_CFLAGS"

AC_CHECK_HEADER(cairo-pdf.h,,AC_MSG_ERROR([
*** Can't find cairo-pdf.h. You must build Cairo with the pdf
*** backend enabled.]))

if test "$os_win32" != "yes"; then
  AC_CHECK_HEADER(cairo-ps.h,,AC_MSG_ERROR([
*** Can't find cairo-ps.h. You must build Cairo with the
*** postscript backend enabled.]))

  AC_CHECK_HEADER(cairo-svg.h,,AC_MSG_ERROR([
*** Cannot find cairo-svg.h. You must build Cairo with the
*** svg backend enabled.]))
fi

CPPFLAGS="$gtk_save_cppflags"


AC_ARG_ENABLE(test-print-backend,
              [AS_HELP_STRING([--enable-test-print-backend],
                              [build test print backend])],,
              [enable_test_print_backend=no])
if test "x$enable_test_print_backend" != "xno" ; then
  PRINT_BACKENDS="$PRINT_BACKENDS test"
fi
AM_CONDITIONAL(TEST_PRINT_BACKEND, test "x$enable_test_print_backend" != "xno")

if test "$os_win32" = "yes"; then
  AC_CHECK_TYPES([IPrintDialogCallback],[],[],[[#include <windows.h>]])
fi

################################################################
# Strip -export-dynamic from the link lines of various libraries
################################################################

#
# pkg-config --libs gmodule includes the "export_dynamic" flag,
#  but this flag is only meaningful for executables. For libraries
#  the effect is undefined; what it causes on Linux is that the
#  export list from -export-symbols-regex is ignored and everything
#  is exported
#
# We are using gmodule-no-export now, but I'm leaving the stripping
# code in place for now, since pango and atk still require gmodule.
export SED
export_dynamic=`(./libtool --config; echo eval echo \\$export_dynamic_flag_spec) | sh`
if test -n "$export_dynamic"; then
  GDK_DEP_LIBS=`echo $GDK_DEP_LIBS | sed -e "s/$export_dynamic//"`
  GTK_DEP_LIBS=`echo $GTK_DEP_LIBS | sed -e "s/$export_dynamic//"`
fi

#############
# GSettings #
#############

GLIB_GSETTINGS

##################################################
# GObject introspection
##################################################

GOBJECT_INTROSPECTION_CHECK(introspection_required_version)

##################################################
# colord module
#################################################

AC_ARG_ENABLE(colord,
              [AS_HELP_STRING([--enable-colord=@<:@yes/no/auto@:>@],
                              [build colord support code [default=auto]])],
              [enable_colord="$enableval"],
              [enable_colord=auto])

AC_MSG_CHECKING([whether to use colord])
if test "$enable_colord" = "auto"; then
  if test "$os_win32" = "yes"; then
    enable_colord=no
  fi
fi
if test "$enable_colord" != "no"; then
  AC_MSG_RESULT([yes])
else
  AC_MSG_RESULT([no])
fi

have_colord=no
if test "$enable_colord" != "no"; then
        if test "$os_win32" != "yes"; then
                PKG_CHECK_MODULES(COLORD, colord >= 0.1.9,
                                  have_colord=yes, have_colord=no)
                if test "$enable_colord" = "yes"; then
                        if test "$have_colord" = "no"; then
                                AC_MSG_ERROR([--enable-colord specified, but not available])
                        fi
                fi
        else
                AC_MSG_ERROR([colord support is not available on win32])
        fi
fi

if test "$have_colord" = "yes"; then
        AC_DEFINE(HAVE_COLORD, 1, [define if we have colord])
fi
AM_CONDITIONAL(HAVE_COLORD, test "x$have_colord" = "xyes")

##################################################
# Checks for gtk-doc and docbook-tools
##################################################

GTK_DOC_CHECK([1.20],[--flavour no-tmpl])

AC_ARG_ENABLE(man,
              [AS_HELP_STRING([--enable-man],
                              [generate man pages [default=auto]])],,
              enable_man=maybe)

if test "$enable_man" != no; then
  AC_PATH_PROG([XSLTPROC], [xsltproc])
  if test -z "$XSLTPROC"; then
    if test "$enable_man" = yes ; then
      AC_MSG_ERROR([xsltproc is required for --enable-man])
    fi
    enable_man=no
  fi
fi

if test "$enable_man" != no; then
  dnl check for DocBook DTD in the local catalog
  JH_CHECK_XML_CATALOG([-//OASIS//DTD DocBook XML V4.1.2//EN],
     [DocBook XML DTD V4.1.2], [have_docbook_dtd=yes], [have_docbook_dtd=no])
  if test "$have_docbook_dtd" != yes; then
    if test "$enable_man" = yes ; then
      AC_MSG_ERROR([DocBook DTD is required for --enable-man])
    fi
    enable_man=no
  fi
fi

if test "$enable_man" != no; then
  dnl check for DocBook XSL stylesheets in the local catalog
  JH_CHECK_XML_CATALOG([http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl],
     [DocBook XSL Stylesheets], [have_docbook_style=yes],[have_docbook_style=no])
  if test "$have_docbook_dtd" != yes; then
    if test "$enable_man" = yes ; then
      AC_MSG_ERROR([DocBook XSL Stylesheets are required for --enable-man])
    fi
    enable_man=no
  fi
fi

AM_CONDITIONAL(ENABLE_MAN, test "$enable_man" != no)

AC_MSG_CHECKING([whether to generate man pages])
if test "$enable_man" != no; then
  AC_MSG_RESULT([yes])
else
  AC_MSG_RESULT([no])
fi

AC_ARG_ENABLE(doc-cross-references,
              AC_HELP_STRING([--disable-doc-cross-references],
                             [cross reference symbols from other libraries @<:@default=yes@:>@]),
              enable_doc_cross_references=$enableval,
              enable_doc_cross_references=yes)

AM_CONDITIONAL(ENABLE_DOC_CROSS_REFERENCES, test x$enable_doc_cross_references != xno)



##################################################
# Visibility handling
##################################################

GDK_HIDDEN_VISIBILITY_CFLAGS=""
case "$host" in
  *-*-mingw*)
    dnl on mingw32 we do -fvisibility=hidden and __declspec(dllexport)
    AC_DEFINE([_GDK_EXTERN], [__attribute__((visibility("default"))) __declspec(dllexport) extern],
              [defines how to decorate public symbols while building])
    CFLAGS="${CFLAGS} -fvisibility=hidden"
    ;;
  *)
    dnl on other compilers, check if we can do -fvisibility=hidden
    SAVED_CFLAGS="${CFLAGS}"
    CFLAGS="-fvisibility=hidden"
    AC_MSG_CHECKING([for -fvisibility=hidden compiler flag])
    AC_TRY_COMPILE([], [int main (void) { return 0; }],
                   AC_MSG_RESULT(yes)
                   enable_fvisibility_hidden=yes,
                   AC_MSG_RESULT(no)
                   enable_fvisibility_hidden=no)
    CFLAGS="${SAVED_CFLAGS}"

    AS_IF([test "${enable_fvisibility_hidden}" = "yes"], [
      AC_DEFINE([_GDK_EXTERN], [__attribute__((visibility("default"))) extern],
                [defines how to decorate public symbols while building])
      GDK_HIDDEN_VISIBILITY_CFLAGS="-fvisibility=hidden"
    ])
    ;;
esac
AC_SUBST(GDK_HIDDEN_VISIBILITY_CFLAGS)

##################################################
# Output commands
##################################################

AC_CONFIG_COMMANDS([gdk/gdkconfig.h], [
	outfile=gdkconfig.h-tmp
	cat > $outfile <<\_______EOF
/* gdkconfig.h
 *
 * This is a generated file.  Please modify `configure.ac'
 */

#ifndef __GDKCONFIG_H__
#define __GDKCONFIG_H__

#if !defined (__GDK_H_INSIDE__) && !defined (GDK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif

#include <glib.h>

G_BEGIN_DECLS

_______EOF

	cat >>$outfile <<_______EOF
$gdk_windowing
_______EOF

	cat >>$outfile <<_______EOF

G_END_DECLS

#endif  /* __GDKCONFIG_H__ */
_______EOF


	if cmp -s $outfile gdk/gdkconfig.h; then
          AC_MSG_NOTICE([gdk/gdkconfig.h is unchanged])
	  rm -f $outfile
	else
	  mv $outfile gdk/gdkconfig.h
	fi
],[
gdk_windowing='$GDK_WINDOWING'
])

dnl
dnl Check for -Bsymbolic-functions linker flag used to avoid
dnl intra-library PLT jumps, if available.
dnl
AC_ARG_ENABLE(Bsymbolic,
              [AS_HELP_STRING([--disable-Bsymbolic],
                              [avoid linking with -Bsymbolic])],,
              [SAVED_LDFLAGS="${LDFLAGS}"
               AC_MSG_CHECKING([for -Bsymbolic-functions linker flag])
               LDFLAGS=-Wl,-Bsymbolic-functions
               AC_LINK_IFELSE([AC_LANG_PROGRAM([[]],
                                               [[int main (void) { return 0; }]])],
                              [AC_MSG_RESULT(yes)
                               enable_Bsymbolic=yes],
                              [AC_MSG_RESULT(no)
                               enable_Bsymbolic=no])
               LDFLAGS="${SAVED_LDFLAGS}"])

if test "x${enable_Bsymbolic}" = "xyes" ; then
  GTK_LINK_FLAGS=-Wl,-Bsymbolic-functions
fi
AC_SUBST(GTK_LINK_FLAGS)

AC_CONFIG_FILES([
config.h.win32
gtk-zip.sh
Makefile
gdk-3.0.pc
gtk+-3.0.pc
gtk+-unix-print-3.0.pc
gail-3.0.pc
m4macros/Makefile
po/Makefile.in
po-properties/Makefile.in
demos/Makefile
demos/gtk-demo/Makefile
demos/gtk-demo/geninclude.pl
demos/widget-factory/Makefile
demos/icon-browser/Makefile
examples/Makefile
examples/bp/Makefile
examples/application1/Makefile
examples/application2/Makefile
examples/application3/Makefile
examples/application4/Makefile
examples/application5/Makefile
examples/application6/Makefile
examples/application7/Makefile
examples/application8/Makefile
examples/application9/Makefile
examples/application10/Makefile
tests/Makefile
tests/visuals/Makefile
testsuite/Makefile
testsuite/a11y/Makefile
testsuite/a11y/state/Makefile
testsuite/css/Makefile
testsuite/css/parser/Makefile
testsuite/gdk/Makefile
testsuite/gtk/Makefile
testsuite/reftests/Makefile
docs/Makefile
docs/reference/Makefile
docs/reference/gdk/Makefile
docs/reference/gdk/version.xml
docs/reference/gtk/Makefile
docs/reference/gtk/gtk3.types
docs/reference/gtk/version.xml
docs/reference/libgail-util/Makefile
docs/reference/libgail-util/version.xml
docs/tools/Makefile
build/Makefile
build/gtk-introspection-msvc.mak
build/win32/Makefile
build/win32/crypt/Makefile
build/win32/vs9/Makefile
build/win32/vs9/gtk-version-paths.vsprops
build/win32/vs10/Makefile
build/win32/vs10/gtk-version-paths.props
build/win32/vs11/Makefile
build/win32/vs12/Makefile
gdk/Makefile
gdk/broadway/Makefile
gdk/x11/Makefile
gdk/win32/Makefile
gdk/win32/rc/Makefile
gdk/win32/rc/gdk.rc
gdk/quartz/Makefile
gdk/wayland/Makefile
gdk/gdkversionmacros.h
gtk/Makefile
gtk/makefile.msc
gtk/gtkversion.h
gtk/gtk-win32.rc
gtk/libgtk3.manifest
gtk/inspector/Makefile
gtk/native/Makefile
util/Makefile
libgail-util/Makefile
modules/Makefile
modules/input/Makefile
modules/printbackends/Makefile
modules/printbackends/cups/Makefile
modules/printbackends/cloudprint/Makefile
modules/printbackends/lpr/Makefile
modules/printbackends/file/Makefile
modules/printbackends/papi/Makefile
modules/printbackends/test/Makefile
])

AC_OUTPUT

# beautify the immodule list a bit
included_immodules=$(echo "${included_immodules}" | $SED 's/,/ /g')
if test -z "${included_immodules}"; then included_immodules="none"; fi

echo ""
echo "        GTK+ $GTK_VERSION"
echo "        ==========="
echo ""
echo "        GDK backends:         $GDK_BACKENDS"
if test "$enable_x11_backend" = "yes"; then
echo "        X11 extensions:       $X_EXTENSIONS"
fi
echo "        Print backends:       $PRINT_BACKENDS"
echo "        Dynamic modules:      $build_dynamic_modules"
echo "        Included immodules:   $included_immodules"
echo "        colord support:       $have_colord"
echo "        Introspection:        $found_introspection"
echo "        Debugging:            $enable_debug"
echo "        Documentation:        $enable_gtk_doc"
