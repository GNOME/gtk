# Process this file with autoconf to produce a configure script.
# Process this file with autoconf to produce a configure script.
# require autoconf 2.54
AC_PREREQ(2.62)

# Making releases:
#   GTK_MICRO_VERSION += 1;
#   GTK_INTERFACE_AGE += 1;
#   GTK_BINARY_AGE += 1;
# if any functions have been added, set GTK_INTERFACE_AGE to 0.
# if backwards compatibility has been broken,
# set GTK_BINARY_AGE and GTK_INTERFACE_AGE to 0.

m4_define([gtk_major_version], [2])
m4_define([gtk_minor_version], [24])
m4_define([gtk_micro_version], [25])
m4_define([gtk_interface_age], [25])
m4_define([gtk_binary_age],
          [m4_eval(100 * gtk_minor_version + gtk_micro_version)])
m4_define([gtk_version],
          [gtk_major_version.gtk_minor_version.gtk_micro_version])
# This is the X.Y used in -lgtk-FOO-X.Y
m4_define([gtk_api_version], [2.0])

# Define a string for the earliest version that this release has
# backwards binary compatibility with for all interfaces a module
# might. Unless we add module-only API with lower stability
# guarantees, this should be unchanged until we break binary compat
# for GTK+.
#
#GTK_BINARY_VERSION=$GTK_MAJOR_VERSION.$GTK_MINOR_VERSION.$LT_CURRENT
m4_define([gtk_binary_version], [2.10.0])

# required versions of other packages
m4_define([glib_required_version], [2.28.0])
m4_define([pango_required_version], [1.20])
m4_define([atk_required_version], [1.29.2])
m4_define([cairo_required_version], [1.6])
m4_define([gdk_pixbuf_required_version], [2.21.0])


AC_INIT([gtk+], [gtk_version],
        [http://bugzilla.gnome.org/enter_bug.cgi?product=gtk%2B],
	[gtk+])

AC_CONFIG_SRCDIR([gdk/gdktypes.h])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

# Save this value here, since automake will set cflags later
cflags_set=${CFLAGS+set}

AM_INIT_AUTOMAKE([no-define -Wno-portability dist-bzip2])

# Support silent build rules, requires at least automake-1.11. Enable
# by either passing --enable-silent-rules to configure or passing V=0
# to make
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([no])])

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

m4_define([gail_lt_current],[18])
m4_define([gail_lt_revision],[1])
m4_define([gail_lt_age],[0])
m4_define([gail_lt_version_info],[gail_lt_current:gail_lt_revision:gail_lt_age])
m4_define([gail_lt_current_minus_age],[m4_eval(gail_lt_current - gail_lt_age)])
AC_SUBST([GAIL_LT_VERSION_INFO],[gail_lt_version_info])
AC_SUBST([GAIL_LT_CURRENT_MINUS_AGE],[gail_lt_current_minus_age])

GETTEXT_PACKAGE=gtk20
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE",
                   [The prefix for our gettext translation domains.])

AC_CANONICAL_HOST

MATH_LIB=-lm
AC_MSG_CHECKING([for native Win32])
LIB_EXE_MACHINE_FLAG=X86
EXE_MANIFEST_ARCHITECTURE=X86
case "$host" in
  *-*-mingw*)
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

AC_SUBST(LIB_EXE_MACHINE_FLAG)
AC_SUBST(EXE_MANIFEST_ARCHITECTURE)

case $host in
  *-*-linux*)
    os_linux=yes
    ;;
esac

dnl Initialize libtool
AC_PROG_CC
AM_DISABLE_STATIC

dnl 
dnl Check for a working C++ compiler, but do not bail out, if none is found.
dnl We use this for an automated test for C++ header correctness.
dnl 
AC_CHECK_TOOLS(CXX, [$CCC c++ g++ gcc CC cxx cc++ cl], gcc)
AC_LANG_SAVE
AC_LANG_CPLUSPLUS

AC_TRY_COMPILE(,[class a { int b; } c;], ,CXX=)
AM_CONDITIONAL(HAVE_CXX, test "$CXX" != "")

gtk_save_cxxflags="$CXXFLAGS"
CXXFLAGS="$CXXFLAGS -x objective-c++"
AC_TRY_COMPILE([@interface Foo @end],,OBJC=yes,OBJC=no)
AM_CONDITIONAL(HAVE_OBJC, test "$OBJC" = "yes")
CXXFLAGS="$gtk_save_cxxflags"
AC_LANG_RESTORE

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

AC_LIBTOOL_WIN32_DLL
AM_PROG_LIBTOOL
dnl when using libtool 2.x create libtool early, because it's used in configure
m4_ifdef([LT_OUTPUT], [LT_OUTPUT])


# Make sure we use 64-bit versions of various file stuff.
AC_SYS_LARGEFILE

AM_PROG_AS
AC_PATH_TOOL(NM, nm, nm)

dnl Initialize maintainer mode
AM_MAINTAINER_MODE([enable])

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
              AC_HELP_STRING([--enable-debug=@<:@no/minimum/yes@:>@],
                             [turn on debugging @<:@default=debug_default@:>@]),,
              enable_debug=debug_default)
AC_ARG_ENABLE(shm,
              [AC_HELP_STRING([--enable-shm],
                              [support shared memory if available [default=yes]])],,
              [enable_shm="yes"])
AC_ARG_ENABLE(xkb,
              [AC_HELP_STRING([--enable-xkb],
                              [support XKB [default=maybe]])],,
              [enable_xkb="maybe"])
AC_ARG_ENABLE(xinerama,
              [AC_HELP_STRING([--enable-xinerama],
	                      [support xinerama extension if available [default=yes]])],,
              [enable_xinerama="yes"])
AC_ARG_ENABLE(rebuilds,
              [AC_HELP_STRING([--disable-rebuilds],
                              [disable all source autogeneration rules])],,
              [enable_rebuilds=yes])
AC_ARG_ENABLE(visibility,
              [AC_HELP_STRING([--disable-visibility],
                              [don't use ELF visibility attributes])],,
              [enable_visibility=yes])

AC_ARG_WITH(xinput,
            [AC_HELP_STRING([--with-xinput=@<:@no/yes@:>@], [support XInput])])

if test "$platform_win32" = yes; then
  gdktarget=win32
else
  gdktarget=x11
fi

AC_ARG_WITH(gdktarget, [  --with-gdktarget=[[x11/win32/quartz/directfb]] select non-default GDK target],
	gdktarget=$with_gdktarget)

AC_SUBST(gdktarget)
case $gdktarget in
  x11|win32|quartz|directfb) ;;
  *) AC_MSG_ERROR([Invalid target for GDK: use x11, quartz, directfb or win32.]);;
esac

gdktargetlib=libgdk-$gdktarget-$GTK_API_VERSION.la
gtktargetlib=libgtk-$gdktarget-$GTK_API_VERSION.la

AC_SUBST(gdktargetlib)
AC_SUBST(gtktargetlib)

if test "x$enable_debug" = "xyes"; then
  test "$cflags_set" = set || CFLAGS="$CFLAGS -g"
  GTK_DEBUG_FLAGS="-DG_ENABLE_DEBUG -DG_ERRORCHECK_MUTEXES" 
else
  if test "x$enable_debug" = "xno"; then
    GTK_DEBUG_FLAGS="-DG_DISABLE_ASSERT -DG_DISABLE_CHECKS -DG_DISABLE_CAST_CHECKS"
  else
    GTK_DEBUG_FLAGS="-DG_DISABLE_CAST_CHECKS"
  fi
fi


if test "x$enable_visibility" = "xno"; then
  GTK_DEBUG_FLAGS="$GTK_DEBUG_FLAGS -DDISABLE_VISIBILITY"
fi


AC_DEFINE_UNQUOTED(GTK_COMPILED_WITH_DEBUGGING, "${enable_debug}",
                   [Define if debugging is enabled])

			
# Build time sanity check...
AM_SANITY_CHECK

# Checks for programs.
AC_ISC_POSIX
AM_PROG_CC_C_O
AC_PROG_INSTALL
AC_PROG_MAKE_SET

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

CPPFLAGS="$CPPFLAGS -DG_DISABLE_SINGLE_INCLUDES -DATK_DISABLE_SINGLE_INCLUDES -DGDK_PIXBUF_DISABLE_SINGLE_INCLUDES -DGTK_DISABLE_SINGLE_INCLUDES"

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

# Honor aclocal flags
ACLOCAL="$ACLOCAL $ACLOCAL_FLAGS"

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
   gdk-pixbuf-2.0 >= gdk_pixbuf_required_version])

## In addition to checking that cairo is present, we also need to
## check that the correct cairo backend is there. E.g. if the GDK
## target is win32 we need the cairo-win32 backend and so on.
cairo_backend=$gdktarget

# GDK calls the xlib backend "x11," cairo calls it "xlib." Other
# backend names are identical.
if test "x$cairo_backend" = "xx11"; then
   cairo_backend=xlib
fi
PKG_CHECK_MODULES(CAIRO_BACKEND,
  [cairo-$cairo_backend >= cairo_required_version])

PKG_CHECK_MODULES(GMODULE, [gmodule-2.0])

if test "$os_win32" != yes; then
    # libtool option to control which symbols are exported
    # right now, symbols starting with _ are not exported
    LIBTOOL_EXPORT_OPTIONS='-export-symbols-regex "^[[^_]].*"'
else
    # We currently use .def files on Windows (for gdk and gtk)
    LIBTOOL_EXPORT_OPTIONS=
fi
AC_SUBST(LIBTOOL_EXPORT_OPTIONS)

dnl ******************************************************
dnl * See whether to include shared library dependencies *
dnl ******************************************************

AC_ARG_ENABLE(explicit-deps, 
              [AC_HELP_STRING([--enable-explicit-deps=@<:@yes/no/auto@:>@],
                              [use explicit dependencies in .pc files [default=auto]])],,
              [enable_explicit_deps=auto])

AC_MSG_CHECKING([Whether to write dependencies into .pc files])
case $enable_explicit_deps in
  auto)
    export SED
    deplibs_check_method=`(./libtool --config; echo 'eval echo \"$deplibs_check_method\"') | sh`
    if test "x$deplibs_check_method" '!=' xpass_all || test "x$enable_static" = xyes ; then
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

# We would like indent, but don't require it.
AC_CHECK_PROG(INDENT, indent, indent)

REBUILD=\#
if test "x$enable_rebuilds" = "xyes" && \
     test -n "$PERL" && \
     $PERL -e 'exit !($] >= 5.002)' > /dev/null 2>&1 ; then
  REBUILD=
fi
AC_SUBST(REBUILD)

AC_CHECK_FUNCS(lstat mkstemp flockfile getc_unlocked)
AC_CHECK_FUNCS(localtime_r)

# _NL_TIME_FIRST_WEEKDAY is an enum and not a define
AC_MSG_CHECKING([for _NL_TIME_FIRST_WEEKDAY])
AC_TRY_LINK([#include <langinfo.h>], [
char c;
c = *((unsigned char *)  nl_langinfo(_NL_TIME_FIRST_WEEKDAY));
], gtk_ok=yes, gtk_ok=no)
AC_MSG_RESULT($gtk_ok)
if test "$gtk_ok" = "yes"; then
  AC_DEFINE([HAVE__NL_TIME_FIRST_WEEKDAY], [1],
	    [Define if _NL_TIME_FIRST_WEEKDAY is available])
fi

# _NL_MEASUREMENT_MEASUREMENT is an enum and not a define
AC_MSG_CHECKING([for _NL_MEASUREMENT_MEASUREMENT])
AC_TRY_LINK([#include <langinfo.h>], [
char c;
c = *((unsigned char *)  nl_langinfo(_NL_MEASUREMENT_MEASUREMENT));
], gtk_ok=yes, gtk_ok=no)
AC_MSG_RESULT($gtk_ok)
if test "$gtk_ok" = "yes"; then
  AC_DEFINE([HAVE__NL_MEASUREMENT_MEASUREMENT], [1],
	    [Define if _NL_MEASUREMENT_MEASUREMENT is available])
fi

# _NL_PAPER_HEIGHT is an enum and not a define
AC_MSG_CHECKING([for _NL_PAPER_HEIGHT])
AC_TRY_LINK([#include <langinfo.h>], [
char c;
c = *((unsigned char *)  nl_langinfo(_NL_PAPER_HEIGHT));
], gtk_ok=yes, gtk_ok=no)
AC_MSG_RESULT($gtk_ok)
if test "$gtk_ok" = "yes"; then
  AC_DEFINE([HAVE__NL_PAPER_HEIGHT], [1],
	    [Define if _NL_PAPER_HEIGHT is available])
fi

# _NL_PAPER_WIDTH is an enum and not a define
AC_MSG_CHECKING([for _NL_PAPER_WIDTH])
AC_TRY_LINK([#include <langinfo.h>], [
char c;
c = *((unsigned char *)  nl_langinfo(_NL_PAPER_WIDTH));
], gtk_ok=yes, gtk_ok=no)
AC_MSG_RESULT($gtk_ok)
if test "$gtk_ok" = "yes"; then
  AC_DEFINE([HAVE__NL_PAPER_WIDTH], [1],
	    [Define if _NL_PAPER_WIDTH is available])
fi
	
# sigsetjmp is a macro on some platforms, so AC_CHECK_FUNCS is not reliable
AC_MSG_CHECKING(for sigsetjmp)
AC_TRY_LINK([#include <setjmp.h>], [
sigjmp_buf env;
sigsetjmp(env, 0);
], gtk_ok=yes, gtk_ok=no)
AC_MSG_RESULT($gtk_ok)
if test "$gtk_ok" = "yes"; then
  AC_DEFINE(HAVE_SIGSETJMP, 1,
            [Define to 1 if sigsetjmp is available])
fi

# i18n stuff
ALL_LINGUAS="`grep -v '^#' "$srcdir/po/LINGUAS" | tr '\n' ' '`"
AM_GLIB_GNU_GETTEXT
LIBS="$LIBS $INTLLIBS"
AC_OUTPUT_COMMANDS([case "$CONFIG_FILES" in *po-properties/Makefile.in*)
        sed -e "/POTFILES =/r po-properties/POTFILES" po-properties/Makefile.in > po-properties/Makefile
      esac])

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
AC_TRY_RUN([#include <math.h>
             int main (void) { return (log(1) != log(1.)); }],
     AC_MSG_RESULT(none needed),
     gtk_save_CFLAGS="$CFLAGS"
     CFLAGS="$CFLAGS -std1"
     AC_TRY_RUN([#include <math.h>
	         int main (void) { return (log(1) != log(1.)); }],
         AC_MSG_RESULT(-std1),
         AC_MSG_RESULT()
         CFLAGS="$gtk_save_CFLAGS"
         AC_MSG_WARN(
                [No ANSI prototypes found in library. (-std1 didn't work.)]),
	 true
     ),
     AC_MSG_RESULT(none needed)
)
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

dnl NeXTStep cc seems to need this
AC_MSG_CHECKING([for extra flags for POSIX compliance])
AC_TRY_COMPILE([#include <dirent.h>], [DIR *dir;],
  AC_MSG_RESULT(none needed),
  gtk_save_CFLAGS="$CFLAGS"
  CFLAGS="$CFLAGS -posix"
  AC_TRY_COMPILE([#include <dirent.h>], [DIR *dir;],
    AC_MSG_RESULT(-posix),
    AC_MSG_RESULT()
    CFLAGS="$gtk_save_CFLAGS"
    AC_MSG_WARN([Could not determine POSIX flag. (-posix didn't work.)])))

#
# Run AM_PATH_GLIB_2_0 to make sure that GLib is installed and working
# 

GLIB_PACKAGES="gobject-2.0 gio-2.0 gmodule-no-export-2.0"

AM_PATH_GLIB_2_0(glib_required_version, :,
  AC_MSG_ERROR([
*** GLIB glib_required_version or better is required. The latest version of 
*** GLIB is always available from ftp://ftp.gtk.org/pub/gtk/.]),
  gobject gmodule-no-export gthread)

# See if it's safe to turn G_DISABLE_DEPRECATED on.
GLIB_VERSION_MAJOR_MINOR=`$PKG_CONFIG --modversion glib-2.0 | sed "s/\.@<:@^.@:>@*\$//"`
GLIB_REQUIRED_VERSION_MAJOR_MINOR=`echo glib_required_version | sed "s/\.@<:@^.@:>@*\$//"`
if test "x$GLIB_VERSION_MAJOR_MINOR" = "x$GLIB_REQUIRED_VERSION_MAJOR_MINOR"; then
  CFLAGS="-DG_DISABLE_DEPRECATED $CFLAGS"
fi


dnl
dnl Check for bind_textdomain_codeset, including -lintl if GLib brings it in.
dnl
gtk_save_LIBS=$LIBS
LIBS="$LIBS $GLIB_LIBS"
AC_CHECK_FUNCS(bind_textdomain_codeset)
LIBS=$gtk_save_LIBS

AC_CHECK_HEADERS(pwd.h,
                 AC_DEFINE(HAVE_PWD_H, 1,
                           [Define to 1 if pwd.h is available]))
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
AC_TRY_COMPILE([#define _XOPEN_SOURCE 500
#define _GNU_SOURCE
#include <ftw.h>], [int flags = FTW_ACTIONRETVAL;], gtk_ok=yes, gtk_ok=no)
if test $gtk_ok = yes; then
    AC_MSG_RESULT([yes])
    AC_DEFINE(HAVE_GNU_FTW, 1, [Have GNU ftw])
else
    AC_MSG_RESULT([no])
fi

saved_cflags="$CFLAGS"
saved_ldflags="$LDFLAGS"


# Checks for header files.
AC_HEADER_STDC

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST

# Checks for library functions.
AC_TYPE_SIGNAL
AC_FUNC_MMAP

AC_CHECK_FUNCS(mallinfo)
AC_CHECK_FUNCS(getresuid)
AC_TYPE_UID_T

# Check if <sys/select.h> needs to be included for fd_set
AC_MSG_CHECKING([for fd_set])
AC_TRY_COMPILE([#include <sys/types.h>],
        [fd_set readMask, writeMask;], gtk_ok=yes, gtk_ok=no)
if test $gtk_ok = yes; then
    AC_MSG_RESULT([yes, found in sys/types.h])
else
    AC_HEADER_EGREP(fd_mask, sys/select.h, gtk_ok=yes)
    if test $gtk_ok = yes; then
        AC_DEFINE(HAVE_SYS_SELECT_H, 1,
                  [Define to 1 if sys/select.h is available])
        AC_MSG_RESULT([yes, found in sys/select.h])
    else
	AC_DEFINE(NO_FD_SET, 1,
                  [Define to 1 if fd_set is not available])
	AC_MSG_RESULT(no)
    fi
fi

# `widechar' tests for gdki18n.h
AC_MSG_CHECKING(for wchar.h)
AC_TRY_CPP([#include <wchar.h>], gdk_wchar_h=yes, gdk_wchar_h=no)
if test $gdk_wchar_h = yes; then
   AC_DEFINE(HAVE_WCHAR_H, 1, [Have wchar.h include file])
fi
AC_MSG_RESULT($gdk_wchar_h)

# Check for wctype.h (for iswalnum)
AC_MSG_CHECKING(for wctype.h)
AC_TRY_CPP([#include <wctype.h>], gdk_wctype_h=yes, gdk_wctype_h=no)
if test $gdk_wctype_h = yes; then
   AC_DEFINE(HAVE_WCTYPE_H, 1, [Have wctype.h include file])
fi
AC_MSG_RESULT($gdk_wctype_h)

# in Solaris 2.5, `iswalnum' is in -lw
GDK_WLIBS=
AC_CHECK_FUNC(iswalnum,,[AC_CHECK_LIB(w,iswalnum,GDK_WLIBS=-lw)])

oLIBS="$LIBS"
LIBS="$LIBS $GDK_WLIBS"
# The following is necessary for Linux libc-5.4.38
AC_MSG_CHECKING(if iswalnum() and friends are properly defined)
AC_TRY_LINK([#include <stdlib.h>],[
#if (defined(HAVE_WCTYPE_H) || defined(HAVE_WCHAR_H))
#  ifdef HAVE_WCTYPE_H
#    include <wctype.h>
#  else
#    ifdef HAVE_WCHAR_H
#      include <wchar.h>
#    endif
#  endif
#else
#  define iswalnum(c) ((wchar_t)(c) <= 0xFF && isalnum(c))
#endif
iswalnum((wchar_t) 0);
], gdk_working_wctype=yes, gdk_working_wctype=no)
LIBS="$oLIBS"

if test $gdk_working_wctype = no; then
   AC_DEFINE(HAVE_BROKEN_WCTYPE, 1, [Is the wctype implementation broken])
   GDK_WLIBS=
fi
AC_MSG_RESULT($gdk_working_wctype)
AC_SUBST(GDK_WLIBS)

# Check for uxtheme.h (for MS-Windows Engine)
AC_MSG_CHECKING(for uxtheme.h)
AC_TRY_CPP([#include <uxtheme.h>], gtk_uxtheme_h=yes, gtk_uxtheme_h=no)
if test $gtk_uxtheme_h = yes; then
   AC_DEFINE(HAVE_UXTHEME_H, 1, [Have uxtheme.h include file])
fi
AC_MSG_RESULT($gtk_uxtheme_h)

# Checks for gdkspawn
AC_CHECK_HEADERS(crt_externs.h)
AC_CHECK_FUNCS(_NSGetEnviron)

AC_MSG_CHECKING(whether to build dynamic modules)

AC_ARG_ENABLE(modules,
              [AC_HELP_STRING([--disable-modules],
                              [disable dynamic module loading])])

dynworks=false
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
        AC_DEFINE(USE_GMODULE, 1,
                  [Define to 1 if gmodule works and should be used])
        AC_MSG_RESULT(yes)
    else
        AC_MSG_RESULT(no)
    fi
fi

AM_CONDITIONAL(BUILD_DYNAMIC_MODULES, $dynworks)

#
# Allow building some or all immodules included
#
AC_MSG_CHECKING(immodules to build)

dnl due to an autoconf bug, commas in the first arg to
dnl AC_HELP_STRING cause problems.
dnl AC_HELP_STRING([--with-included-immodules=MODULE1 MODULE2 ...],
dnl                [build the specified input method modules into gtk])
AC_ARG_WITH(included_immodules,
[  --with-included-immodules=MODULE1,MODULE2,...
                          build the specified input methods into gtk])

if $dynworks; then 
   :
else
   ## if the option was specified, leave it; otherwise disable included immodules
   if test x$with_included_immodules = xno; then
           with_included_immodules=yes
   fi
fi

all_immodules="am-et,cedilla,cyrillic-translit"
if test "$gdktarget" = "win32"; then
   all_immodules="${all_immodules},ime"
fi
if test "$gdktarget" = "quartz"; then
   all_immodules="${all_immodules},quartz"
fi
all_immodules="${all_immodules},inuktitut,ipa,multipress,thai,ti-er,ti-et,viqr"
if test "$gdktarget" = "x11"; then
   all_immodules="${all_immodules},xim"
fi

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
AM_CONDITIONAL(INCLUDE_IM_THAI, [test x"$INCLUDE_thai" = xyes])
AM_CONDITIONAL(INCLUDE_IM_TI_ER, [test x"$INCLUDE_ti_er" = xyes])
AM_CONDITIONAL(INCLUDE_IM_TI_ET, [test x"$INCLUDE_ti_et" = xyes])
AM_CONDITIONAL(INCLUDE_IM_VIQR, [test x"$INCLUDE_viqr" = xyes])
AM_CONDITIONAL(INCLUDE_IM_XIM, [test x"$INCLUDE_xim" = xyes])

AC_HEADER_SYS_WAIT

AC_TYPE_SIGNAL

# Checks to see whether we should include mediaLib
# support.
#
AC_CHECK_HEADER(sys/systeminfo.h,
                AC_DEFINE(HAVE_SYS_SYSTEMINFO_H, 1, 
                          [Define to 1 if sys/systeminfo.h is available]))
AC_CHECK_HEADER(sys/sysinfo.h,
                AC_DEFINE(HAVE_SYS_SYSINFO_H, 1,
                          [Define to 1 if sys/sysinfo.h is available]))

AC_MSG_CHECKING(for mediaLib 2.3)
use_mlib25=no
# Check for a mediaLib 2.3 function since that is what the GTK+ mediaLib
# patch requires.
AC_CHECK_LIB(mlib, mlib_ImageSetStruct, use_mlib=yes, use_mlib=no)
if test $use_mlib = yes; then
    AC_DEFINE(USE_MEDIALIB, 1,
              [Define to 1 if medialib is available and should be used])
    MEDIA_LIB=-lmlib

    AC_MSG_CHECKING(for mediaLib 2.5)
    # Check for a mediaLib 2.5 function since that is what is needed for
    # gdk_rgb_convert integration.
    AC_CHECK_LIB(mlib, mlib_VideoColorRGBint_to_BGRAint, use_mlib25=yes, use_mlib25=no)
    if test $use_mlib25 = yes; then
        AC_DEFINE(USE_MEDIALIB25, 1,
                  [Define to 1 if medialib 2.5 is available])
    fi
fi
AM_CONDITIONAL(USE_MEDIALIB, test $use_mlib = yes)
AM_CONDITIONAL(USE_MEDIALIB25, test $use_mlib25 = yes)

dnl Look for a host system's gdk-pixbuf-csource if we are cross-compiling

AM_CONDITIONAL(CROSS_COMPILING, test $cross_compiling = yes)

if test $cross_compiling = yes; then
  AC_PATH_PROG(GTK_UPDATE_ICON_CACHE, gtk-update-icon-cache, no)
  if test x$GTK_UPDATE_ICON_CACHE = xno; then
    REBUILD_PNGS=#
  fi
fi

AC_PATH_PROG(GDK_PIXBUF_CSOURCE, gdk-pixbuf-csource, no)

if test ! -f $srcdir/gtk/gtkbuiltincache.h && 
   test "x$REBUILD_PNGS" = "x#" ; then
     AC_MSG_ERROR([
*** gtkbuiltincache.h is not in the tree, and cannot be built
*** because you don't have libpng, or (when cross-compiling) you 
*** don't have a prebuilt gtk-update-icon-cache on the build system.])
fi

########################################
# Windowing system checks
########################################

GDK_EXTRA_LIBS="$GDK_WLIBS"
GDK_EXTRA_CFLAGS=
	
# GTK+ uses some X calls, so needs to link against X directly
GTK_DEP_PACKAGES_FOR_X=
GTK_DEP_LIBS_FOR_X=

if test "x$gdktarget" = "xx11"; then
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
  if $PKG_CONFIG --exists x11 xext xrender; then
    have_base_x_pc=true
    X_PACKAGES="$X_PACKAGES x11 xext xrender"
    x_libs="`$PKG_CONFIG --libs x11 xext xrender`"
    X_CFLAGS="`$PKG_CONFIG --cflags x11 xext xrender`"
    
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
    x_libs_for_checks="$X_LIBS -lXext -lXrender -lX11 $X_EXTRA_LIBS"

    GTK_DEP_LIBS_FOR_X="$X_LIBS -lXrender -lX11 $X_EXTRA_LIBS"
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
                AC_MSG_ERROR([*** libX11 not found. Check 'config.log' for more details.]))
  AC_CHECK_FUNC(XextFindDisplay, :,
                AC_MSG_ERROR([*** libXext not found. Check 'config.log' for more details.]))
  AC_CHECK_FUNC(XRenderQueryExtension, :,
                AC_MSG_ERROR([*** libXrender not found. Check 'config.log' for more details.]))

  # Check for xReply

  AC_MSG_CHECKING([if <X11/extensions/XIproto.h> is needed for xReply])
  AC_TRY_COMPILE([#include <X11/Xlibint.h>],
      [xReply *rep;],
      [AC_MSG_RESULT([no])],
      [AC_TRY_COMPILE([#include <X11/extensions/XIproto.h>
#include <X11/Xlibint.h>],
           [xReply *rep;],
           [AC_MSG_RESULT([yes])
            AC_DEFINE([NEED_XIPROTO_H_FOR_XREPLY], 1,
                      [Define if <X11/extensions/XIproto.h> needed for xReply])],
           [AC_MSG_RESULT([unknown])
            AC_MSG_ERROR([xReply type unavailable. X11 is too old])])])

  # Check for XConvertCase, XInternAtoms (X11R6 specific)

  AC_CHECK_FUNCS(XConvertCase XInternAtoms)

  # Generic X11R6 check needed for XIM support; we could
  # probably use this to replace the above, but we'll
  # leave the separate checks for XConvertCase and XInternAtoms 
  # for clarity
  
  have_x11r6=false
  AC_CHECK_FUNC(XAddConnectionWatch,
      have_x11r6=true)

  if $have_x11r6; then
    AC_DEFINE(HAVE_X11R6, 1, [Define if we have X11R6])
  fi
  AM_CONDITIONAL(HAVE_X11R6, $have_x11r6)

  # Check for XKB support.

  if test "x$enable_xkb" = "xyes"; then
        AC_MSG_WARN(XKB support explicitly enabled)
        AC_DEFINE(HAVE_XKB, 1, [Define to use XKB extension])
  elif test "x$enable_xkb" = "xmaybe"; then
        AC_CHECK_FUNC(XkbQueryExtension,
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

  # Xshm checks

  if test "x$enable_shm" = "xyes"; then
     # Check for the XShm extension, normally in Xext
     AC_CHECK_FUNC(XShmAttach,
	:,
	# On AIX, it is in XextSam instead
	[AC_CHECK_LIB(XextSam, XShmAttach, 
	    [GTK_ADD_LIB(x_extra_libs,XextSam)])])
  fi    

  if test "x$enable_shm" = "xyes"; then
    # Check for shared memory
    AC_CHECK_HEADER(sys/ipc.h,
                    AC_DEFINE(HAVE_IPC_H, 1,
                              [Define to 1 if ipc.h is available]),
                    no_sys_ipc=yes)
    AC_CHECK_HEADER(sys/shm.h,
                    AC_DEFINE(HAVE_SHM_H, 1,
                              [Define to 1 if shm.h is available]),
                    no_sys_shm=yes)

    # Check for the X shared memory extension header file
    have_xshm=no 
    AC_MSG_CHECKING(X11/extensions/XShm.h)
    if test "x$no_xext_lib" = "xyes"; then
      :
    else
      gtk_save_CFLAGS="$CFLAGS"
      CFLAGS="$CFLAGS $x_cflags"
      AC_TRY_COMPILE([
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>
], [XShmSegmentInfo *x_shm_info;], have_xshm=yes)
      CFLAGS="$gtk_save_CFLAGS"
    fi
    AC_MSG_RESULT($have_xshm)
    if test $have_xshm = yes ; then
      AC_DEFINE(HAVE_XSHM_H, 1,
                [Define to 1 if xshm.h is available])
    fi
  fi

  if test "x$enable_xinerama" = "xyes"; then
    # Check for Xinerama extension (Solaris impl or Xfree impl)
    gtk_save_cppflags="$CPPFLAGS"
    CPPFLAGS="$CPPFLAGS $x_cflags"
  
    # Check for XFree
    AC_MSG_CHECKING(for Xinerama support on XFree86)
	  
    have_xfree_xinerama=false
    if $PKG_CONFIG --exists xinerama ; then
       have_xfree_xinerama=true
       X_PACKAGES="$X_PACKAGES xinerama"
    else    
       AC_CHECK_LIB(Xinerama, XineramaQueryExtension,
	   [AC_CHECK_HEADER(X11/extensions/Xinerama.h, 
	   [GTK_ADD_LIB(x_extra_libs,Xinerama)
	   have_xfree_xinerama=true], :,
           [#include <X11/Xlib.h>])])
    fi

    if $have_xfree_xinerama ; then
      AC_DEFINE(HAVE_XFREE_XINERAMA, 1,
                [Define to 1 if XFree Xinerama is available])
      AC_DEFINE(HAVE_XINERAMA, 1,
                [Define to 1 is Xinerama is available])
      AC_MSG_RESULT(yes)
    else
      AC_MSG_RESULT(no)

      case "$host" in
        *-*-solaris*)
            # Check for solaris
	    AC_MSG_CHECKING(for Xinerama support on Solaris)
	  
	    have_solaris_xinerama=false
	    AC_CHECK_FUNC(XineramaGetInfo,
	        [AC_CHECK_HEADER(X11/extensions/xinerama.h,
  	  	    [have_solaris_xinerama=true], :,
		    [#include <X11/Xlib.h>])])
		
            if $have_solaris_xinerama ; then
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
  
  # set up things for XInput

  if test "x$with_xinput" = "xxfree" || test "x$with_xinput" = "xyes"; then
    AC_DEFINE(XINPUT_XFREE, 1,
              [Define to 1 if XFree XInput should be used])
    
    if $PKG_CONFIG --exists xi ; then
      X_PACKAGES="$X_PACKAGES xi"
    else
      GTK_ADD_LIB(x_extra_libs, Xi)
    fi
  else
    AC_DEFINE(XINPUT_NONE, 1,
              [Define to 1 if no XInput should be used])
  fi

  AM_CONDITIONAL(XINPUT_XFREE, test x$with_xinput = xxfree || test x$with_xinput = xyes)

  # Check for the RANDR extension
  if $PKG_CONFIG --exists "xrandr >= 1.2.99" ; then
     AC_DEFINE(HAVE_RANDR, 1, [Have the Xrandr extension library])

     X_PACKAGES="$X_PACKAGES xrandr"
  fi
       
  # Checks for Xcursor library
  
  if $PKG_CONFIG --exists xcursor ; then
    AC_DEFINE(HAVE_XCURSOR, 1, [Have the Xcursor library])
    
    X_PACKAGES="$X_PACKAGES xcursor"
  fi

  # Checks for XFixes extension
  
  if $PKG_CONFIG --exists xfixes ; then
    AC_DEFINE(HAVE_XFIXES, 1, [Have the XFIXES X extension])
    
    X_PACKAGES="$X_PACKAGES xfixes"
    GTK_PACKAGES_FOR_X="$GTK_PACKAGES_FOR_X xfixes"
  fi

  # Checks for Xcomposite extension

  if $PKG_CONFIG --exists xcomposite ; then
    AC_DEFINE(HAVE_XCOMPOSITE, 1, [Have the XCOMPOSITE X extension])

    X_PACKAGES="$X_PACKAGES xcomposite"
    GTK_PACKAGES_FOR_X="$GTK_PACKAGES_FOR_X xcomposite"
  fi

  # Checks for Xdamage extension

  if $PKG_CONFIG --exists xdamage ; then
    AC_DEFINE(HAVE_XDAMAGE, 1, [Have the XDAMAGE X extension])

    X_PACKAGES="$X_PACKAGES xdamage"
    GTK_PACKAGES_FOR_X="$GTK_PACKAGES_FOR_X xdamage"
  fi

  if $have_base_x_pc ; then
    GDK_EXTRA_LIBS="$x_extra_libs"
  else
    GDK_EXTRA_LIBS="$X_LIBS $x_extra_libs -lXext -lX11 $GDK_EXTRA_LIBS"
  fi

  CPPFLAGS="$gtk_save_cppflags"
  LIBS="$gtk_save_libs"

  AM_CONDITIONAL(USE_X11, true)
else
  XPACKAGES=

  AM_CONDITIONAL(XINPUT_XFREE, false)
  AM_CONDITIONAL(USE_X11, false)
  AM_CONDITIONAL(HAVE_X11R6, false)
fi

if test "x$gdktarget" = "xwin32"; then
  GDK_EXTRA_LIBS="$GDK_EXTRA_LIBS -lgdi32 -limm32 -lshell32 -lole32 -Wl,-luuid"
  AM_CONDITIONAL(USE_WIN32, true)
else
  AM_CONDITIONAL(USE_WIN32, false)
fi

AC_ARG_ENABLE(quartz-relocation,
              [AS_HELP_STRING([--enable-quartz-relocation],
                              [enable bundle-based relocation functions])],
                              [quartz_relocation=yes])

if test "x$gdktarget" = "xquartz"; then
  GDK_EXTRA_LIBS="$GDK_EXTRA_LIBS -framework Cocoa"
  AM_CONDITIONAL(USE_QUARTZ, true)
  if test "x$quartz_relocation" = xyes; then
    AC_DEFINE([QUARTZ_RELOCATION], [1], [Use NSBundle functions to determine load paths for libraries, translations, etc.])
  fi

else
  AM_CONDITIONAL(USE_QUARTZ, false)
fi

if test "x$gdktarget" = "xdirectfb"; then
  DIRECTFB_REQUIRED_VERSION=1.0.0
  AC_MSG_CHECKING(for DirectFB)

  PKG_CHECK_MODULES(DIRECTFB, [directfb >= $DIRECTFB_REQUIRED_VERSION])
  AM_CONDITIONAL(USE_DIRECTFB, true)
else
  AM_CONDITIONAL(USE_DIRECTFB, false)
fi


# Check for Pango flags

if test "x$gdktarget" = "xwin32"; then
	PANGO_PACKAGES="pangowin32 pangocairo"
else
	PANGO_PACKAGES="pango pangocairo"
fi

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

CFLAGS="$saved_cflags"
LDFLAGS="$saved_ldflags"

# Pull in gio-unix for GDesktopAppInfo usage, see at least gdkapplaunchcontext-x11.c
if test "x$gdktarget" = "xx11"; then
  GDK_PACKAGES="$PANGO_PACKAGES gio-unix-2.0 $X_PACKAGES gdk-pixbuf-2.0 cairo-$cairo_backend"
else
  GDK_PACKAGES="$PANGO_PACKAGES gio-2.0 gdk-pixbuf-2.0 cairo-$cairo_backend"
fi

GDK_DEP_LIBS="$GDK_EXTRA_LIBS `$PKG_CONFIG --libs $GDK_PACKAGES` $MATH_LIB"
GDK_DEP_CFLAGS="`$PKG_CONFIG --cflags  gthread-2.0 $GDK_PACKAGES` $GDK_EXTRA_CFLAGS"
#
# If we aren't writing explicit dependencies, then don't put the extra libraries we need
# into the pkg-config files
#
if test $enable_explicit_deps != yes ; then
  GDK_PACKAGES="$PANGO_PACKAGES gdk-pixbuf-2.0"
  GDK_EXTRA_LIBS=
fi

AC_SUBST(GDK_PACKAGES)
AC_SUBST(GDK_EXTRA_LIBS)
AC_SUBST(GDK_EXTRA_CFLAGS)
AC_SUBST(GDK_DEP_LIBS)
AC_SUBST(GDK_DEP_CFLAGS)


########################################
# Check for Accessibility Toolkit flags
########################################

ATK_PACKAGES=atk
AC_MSG_CHECKING(ATK flags)
if $PKG_CONFIG --exists $ATK_PACKAGES ; then
        ATK_CFLAGS=`$PKG_CONFIG --cflags $ATK_PACKAGES`
        ATK_LIBS=`$PKG_CONFIG --libs $ATK_PACKAGES`

        AC_MSG_RESULT($ATK_CFLAGS $ATK_LIBS)
else
        AC_MSG_ERROR([
*** Accessibility Toolkit not found. Accessibility Toolkit is required
*** to build GTK+.
])
fi

if $PKG_CONFIG --uninstalled $ATK_PACKAGES; then
        :
else
	gtk_save_LIBS="$LIBS"
        LIBS="$ATK_LIBS $LIBS"
        AC_TRY_LINK_FUNC(atk_object_get_type, : , AC_MSG_ERROR([
                *** Cannot link to Accessibility Toolkit. Accessibility Toolkit is required
                *** to build GTK+]))
        LIBS="$gtk_save_LIBS"
fi

GTK_PACKAGES="atk cairo gdk-pixbuf-2.0 gio-2.0"
if test "x$gdktarget" = "xx11"; then
  GTK_PACKAGES="$GTK_PACKAGES pangoft2"
fi
GTK_EXTRA_LIBS=
GTK_EXTRA_CFLAGS= 
GTK_DEP_LIBS="$GDK_EXTRA_LIBS $GTK_DEP_LIBS_FOR_X `$PKG_CONFIG --libs $PANGO_PACKAGES $GTK_PACKAGES_FOR_X $GTK_PACKAGES` $GTK_EXTRA_LIBS $MATH_LIB"
GTK_DEP_CFLAGS="`$PKG_CONFIG --cflags  gthread-2.0 $GDK_PACKAGES $GTK_PACKAGES` $GDK_EXTRA_CFLAGS $GTK_EXTRA_CFLAGS"

if test x"$os_win32" = xyes; then
  GTK_EXTRA_CFLAGS="$msnative_struct"
fi

GLIB_PREFIX="`$PKG_CONFIG --variable=prefix glib-2.0`"
ATK_PREFIX="`$PKG_CONFIG --variable=prefix atk`"
PANGO_PREFIX="`$PKG_CONFIG --variable=prefix pango`"
CAIRO_PREFIX="`pkg-config --variable=prefix cairo`"

AC_SUBST(GTK_PACKAGES)
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

AC_ARG_ENABLE(cups,
              [AC_HELP_STRING([--disable-cups]
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
            $CUPS_API_MAJOR -eq 1 -a $CUPS_API_MINOR -ge 2; then
      AC_DEFINE(HAVE_CUPS_API_1_2, 1, 
                [Define to 1 if CUPS 1.2 API is available])
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

    AM_CONDITIONAL(HAVE_CUPS, true)

    gtk_save_cflags="$CFLAGS"
    CFLAGS="$CUPS_CFLAGS"
    AC_TRY_COMPILE([#include <cups/http.h>],
                   [http_t http; char *s = http.authstring;],
                   [AC_DEFINE(HAVE_HTTP_AUTHSTRING, [],
                              [Define if cups http_t authstring field is accessible])],)
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
              [AC_HELP_STRING([--disable-papi]
                              [disable papi print backend])],,
             [enable_papi=auto])

if test "x$enable_papi" = "xno"; then
  AM_CONDITIONAL(HAVE_PAPI, false)
else
  AC_MSG_CHECKING(libpapi)
  AC_CHECK_LIB(papi, papiServiceCreate, have_papi=yes, have_papi=no)
  if test $have_papi = yes; then
    AC_DEFINE([HAVE_PAPI], [], [Define to 1 if libpapi available])
  fi
  AM_CONDITIONAL(HAVE_PAPI, test $have_papi = yes)
  if test "x$enable_papi" = "xyes" -a "x$have_papi" = "xno"; then
    AC_MSG_ERROR([
*** papi not found.
])
  fi
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
*** Can't find cairo-svg.h. You must build Cairo with the
*** svg backend enabled.]))
fi   

CPPFLAGS="$gtk_save_cppflags"
     
			  	
AC_ARG_ENABLE(test-print-backend,
              [AC_HELP_STRING([--enable-test-print-backend],
                              [build test print backend])],,
              [enable_test_print_backend=no])
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

##################################################
# GObject introspection
##################################################

GOBJECT_INTROSPECTION_CHECK([0.9.3])

##################################################
# Checks for gtk-doc and docbook-tools
##################################################

GTK_DOC_CHECK([1.11])

AC_CHECK_PROG(DB2HTML, db2html, true, false)
AM_CONDITIONAL(HAVE_DOCBOOK, $DB2HTML)

AC_ARG_ENABLE(man,
              [AC_HELP_STRING([--enable-man],
                              [regenerate man pages from Docbook [default=no]])],,
              enable_man=no)

if test "${enable_man}" != no; then
  dnl
  dnl Check for xsltproc
  dnl
  AC_PATH_PROG([XSLTPROC], [xsltproc])
  if test -z "$XSLTPROC"; then
    enable_man=no
  fi

  dnl check for DocBook DTD and stylesheets in the local catalog.
  JH_CHECK_XML_CATALOG([-//OASIS//DTD DocBook XML V4.1.2//EN],
     [DocBook XML DTD V4.1.2],,enable_man=no)
  JH_CHECK_XML_CATALOG([http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl],
     [DocBook XSL Stylesheets],,enable_man=no)
fi

AM_CONDITIONAL(ENABLE_MAN, test x$enable_man != xno)


##################################################
# Output commands
##################################################

AC_CONFIG_COMMANDS([gdk/gdkconfig.h], [
	outfile=gdkconfig.h-tmp
	cat > $outfile <<\_______EOF
/* gdkconfig.h
 *
 * This is a generated file.  Please modify `configure.in'
 */

#ifndef GDKCONFIG_H
#define GDKCONFIG_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef GSEAL
/* introduce GSEAL() here for all of Gdk and Gtk+ without the need to modify GLib */
#  ifdef GSEAL_ENABLE
#    define GSEAL(ident)      _g_sealed__ ## ident
#  else
#    define GSEAL(ident)      ident
#  endif
#endif /* !GSEAL */

_______EOF

	cat >>$outfile <<_______EOF
$gdk_windowing
$gdk_wc
_______EOF

	cat >>$outfile <<_______EOF

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* GDKCONFIG_H */
_______EOF


	if cmp -s $outfile gdk/gdkconfig.h; then
          AC_MSG_NOTICE([gdk/gdkconfig.h is unchanged])
	  rm -f $outfile
	else
	  mv $outfile gdk/gdkconfig.h
	fi
],[
if test "x$gdktarget" = "xx11" ; then
  gdk_windowing='
#define GDK_WINDOWING_X11'
elif test "x$gdktarget" = "xwin32" ; then
  gdk_windowing='
#define GDK_NATIVE_WINDOW_POINTER

#define GDK_WINDOWING_WIN32'
elif test "x$gdktarget" = "xquartz" ; then
  gdk_windowing='
#define GDK_NATIVE_WINDOW_POINTER

#define GDK_WINDOWING_QUARTZ'
elif test "x$gdktarget" = "xdirectfb" ; then
  gdk_windowing='
#define GDK_WINDOWING_DIRECTFB'
fi

if test x$gdk_wchar_h = xyes; then
  gdk_wc='
#define GDK_HAVE_WCHAR_H 1'
fi
if test x$gdk_wctype_h = xyes; then
  gdk_wc="\$gdk_wc
#define GDK_HAVE_WCTYPE_H 1"
fi
if test x$gdk_working_wctype = xno; then
  gdk_wc="\$gdk_wc
#define GDK_HAVE_BROKEN_WCTYPE 1"
fi


])

AC_CONFIG_FILES([
config.h.win32
gtk-zip.sh
Makefile
gdk-2.0.pc
gtk+-2.0.pc
gtk+-unix-print-2.0.pc
gail.pc
gdk-2.0-uninstalled.pc
gtk+-2.0-uninstalled.pc
gail-uninstalled.pc
m4macros/Makefile
po/Makefile.in
po-properties/Makefile.in
demos/Makefile
demos/gtk-demo/Makefile
demos/gtk-demo/geninclude.pl
tests/Makefile
docs/Makefile
docs/reference/Makefile
docs/reference/gdk/Makefile
docs/reference/gdk/version.xml
docs/reference/gtk/Makefile
docs/reference/gtk/version.xml
docs/reference/libgail-util/Makefile
docs/faq/Makefile
docs/tools/Makefile
docs/tutorial/Makefile
build/Makefile
build/win32/Makefile
build/win32/vs9/Makefile
build/win32/vs10/Makefile
build/win32/vs11/Makefile
build/win32/vs12/Makefile
gdk/Makefile
gdk/x11/Makefile
gdk/win32/Makefile
gdk/win32/rc/Makefile
gdk/win32/rc/gdk.rc
gdk/quartz/Makefile
gdk/directfb/Makefile
gdk/tests/Makefile
gtk/Makefile
gtk/makefile.msc
gtk/gtkversion.h
gtk/gtk-win32.rc
gtk/theme-bits/Makefile
gtk/tests/Makefile
modules/Makefile
modules/other/Makefile
modules/other/gail/Makefile
modules/other/gail/libgail-util/Makefile
modules/other/gail/tests/Makefile
modules/engines/Makefile
modules/engines/pixbuf/Makefile
modules/engines/ms-windows/Makefile
modules/engines/ms-windows/Theme/Makefile
modules/engines/ms-windows/Theme/gtk-2.0/Makefile
modules/input/Makefile
modules/printbackends/Makefile
modules/printbackends/cups/Makefile
modules/printbackends/lpr/Makefile
modules/printbackends/file/Makefile
modules/printbackends/papi/Makefile
modules/printbackends/test/Makefile
perf/Makefile
])

AC_OUTPUT

echo "configuration:
        target: $gdktarget"
