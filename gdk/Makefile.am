## Makefile.am for gtk+/gdk

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
libgdkincludedir = $(includedir)/gtk-1.2/gdk
libgdk_la_LDFLAGS = @STRIP_BEGIN@ \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
	-release $(LT_RELEASE) \
	-export-dynamic \
	@GLIB_DEPLIBS@	\
	@x_ldflags@	\
	@x_libs@	\
	-lm		\
@STRIP_END@

#
# setup source file variables
#
# GDK header files for public installation (non-generated)
gdk_public_h_sources = @STRIP_BEGIN@ \
	gdk.h		\
	gdkcursors.h	\
	gdkrgb.h	\
	gdki18n.h	\
	gdkkeysyms.h	\
	gdkprivate.h	\
	gdktypes.h	\
	gdkx.h		\
@STRIP_END@
gdk_c_sources = @STRIP_BEGIN@ \
	gdk.c		\
	gdkcc.c		\
	gdkcolor.c	\
	gdkcursor.c	\
	gdkdnd.c	\
	gdkdraw.c	\
	gdkevents.c     \
	gdkfont.c	\
	gdkgc.c		\
	gdkglobals.c	\
	gdkim.c		\
	gdkimage.c	\
	gdkinput.c	\
	gdkinput.h	\
	gdkinputnone.h  \
	gdkinputcommon.h\
	gdkinputgxi.h	\
	gdkinputxfree.h	\
	gdkpixmap.c	\
	gdkproperty.c	\
	gdkrgb.c	\
	gdkrectangle.c	\
	gdkregion.c	\
	gdkselection.c	\
	gdkvisual.c	\
	gdkwindow.c	\
	gdkxid.c 	\
	MwmUtil.h       \
	gxid_lib.h	\
	gxid_proto.h	\
        gxid_lib.c	\
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
# extra programs
#
EXTRA_PROGRAMS = gxid
bin_PROGRAMS = @xinput_progs@
LDADDS = @STRIP_BEGIN@ \
	@x_ldflags@	\
	@x_libs@	\
	@GLIB_LIBS@	\
	-lm		\
@STRIP_END@
gxid_SOURCES = gxid.c
gxid_LDADD = $(LDADDS)


.PHONY: files

files:
	@files=`ls $(DISTFILES) 2> /dev/null `; for p in $$files; do \
	  echo $$p; \
	done
