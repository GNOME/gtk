## Makefile.am for gtk+/gdk

SUBDIRS=x11 win32

EXTRA_DIST =	\
	gdkconfig.h.win32 \
	gdk.def \
	makefile.cygwin \
	makefile.msc

INCLUDES = @STRIP_BEGIN@ \
	-DG_LOG_DOMAIN=\"Gdk\"	\
	-I$(top_srcdir)		\
	@GTK_DEBUG_FLAGS@ 	\
	@GTK_XIM_FLAGS@		\
	@GTK_LOCALE_FLAGS@	\
	@GLIB_CFLAGS@		\
	@x_cflags@ 		\
@STRIP_END@

#
# libraries to compile and install
#
lib_LTLIBRARIES = libgdk.la

# libtool stuff: set version and export symbols for resolving
libgdkincludedir = $(includedir)/gdk
libgdk_la_LDFLAGS = @STRIP_BEGIN@ \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
	-release $(LT_RELEASE) \
	-export-dynamic \
	@GLIB_DEPLIBS@	\
	@x_ldflags@	\
	@x_libs@	\
	-lm		\
@STRIP_END@

libgdk_la_LIBADD = \
	x11/libgdk-x11.la	

#
# setup source file variables
#
#
# GDK header files for public installation (non-generated)
#
# Note: files added here may need to be be propagated to gdk_headers in gtk/Makefile.am
#
gdk_public_h_sources = @STRIP_BEGIN@ \
	gdk.h		\
	gdkcc.h		\
	gdkcolor.h	\
	gdkcompat.h	\
	gdkcursor.h     \
	gdkcursors.h	\
	gdkdnd.h	\
	gdkdrawable.h	\
	gdkevents.h	\
	gdkfont.h	\
	gdkgc.h		\
	gdkkeysyms.h	\
	gdki18n.h	\
	gdkim.h		\
	gdkimage.h	\
	gdkinput.h	\
	gdkpixmap.h	\
	gdkprivate.h	\
	gdkproperty.h	\
	gdkregion.h	\
	gdkrgb.h	\
	gdkselection.h	\
	gdktypes.h	\
	gdkvisual.h	\
	gdkwindow.h	\
@STRIP_END@
gdk_c_sources = @STRIP_BEGIN@ \
	gdk.c		\
	gdkcolor.c	\
	gdkcursor.c	\
	gdkdraw.c	\
	gdkevents.c     \
	gdkfont.c	\
	gdkgc.c		\
	gdkglobals.c	\
	gdkimage.c	\
	gdkrgb.c	\
	gdkrectangle.c	\
	gdkwindow.c	\
@STRIP_END@

#
# setup GDK sources and their dependancies
#
libgdkinclude_HEADERS = $(gdk_public_h_sources)
libgdk_la_SOURCES = $(gdk_c_sources)
MAINTAINERCLEANFILES +=
EXTRA_HEADERS +=
EXTRA_DIST +=
EXTRA_DIST +=

#
# rules to generate built sources
#
# we only need to remake these headers once a new X version is released
X-derived-headers:
	sed -e 's/^#define[ 	]*XC\([^ 	]*\)[ 	]*\([^ 	]*\)[ 	]*.*$$/GDK\1 = \2,/' \
	    -e 'tb' -e 'd' -e ':b' \
	    -e 'y/abcdefghijklmnopqrstuvwxyz/ABCDEFGHIJKLMNOPQRSTUVWXYZ/' \
	    < @x_includes@/X11/cursorfont.h > gdkcursors.h ; \
	sed -e 's/^#define[ 	]*XK\([^ 	]*\)[ 	]*\([^ 	]*\)[ 	]*.*$$/#define GDK\1 \2/' \
	    -e 'tb' -e 'd' -e ':b' -e 's/ 0X/ 0x/' \
	    < @x_includes@/X11/keysymdef.h > gdkkeysyms.h

#
# Rule to install gdkconfig.h header file
#
configexecincludedir = $(pkglibdir)/include
#configexecinclude_DATA = gdkconfig.h
install-exec-local: gdkconfig.h
	$(mkinstalldirs) $(DESTDIR)$(configexecincludedir)
	file=$(DESTDIR)$(configexecincludedir)/gdkconfig.h; \
	if test -r $$file && cmp -s gdkconfig.h $$file; then :; \
	else $(INSTALL_DATA) gdkconfig.h $$file; fi

BUILT_SOURCES = stamp-gc-h #note: not gdkconfig.h
gdkconfig.h: stamp-gc-h
	@if test -f gdkconfig.h; then :; \
	else rm -f stamp-gc-h; $(MAKE) stamp-gc-h; fi
stamp-gc-h: ../config.status
	cd .. && CONFIG_FILES= CONFIG_HEADERS= CONFIG_OTHER=gdk/gdkconfig.h ./config.status
	echo timestamp > stamp-gc-h

.PHONY: files

files:
	@files=`ls $(DISTFILES) 2> /dev/null `; for p in $$files; do \
	  echo $$p; \
	done
