## Process this file with automake to produce Makefile.in

INCLUDES = @STRIP_BEGIN@ 	\
	-DG_LOG_DOMAIN=\"Gdk\"	\
	-DINSIDE_GDK_WIN32	\
	-I$(top_srcdir)		\
	-I$(top_srcdir)/gdk	\
	@x_includes@		\
	@GTK_DEBUG_FLAGS@ 	\
	@GTK_LOCALE_FLAGS@	\
	@GLIB_CFLAGS@		\
	-DGDK_COMPILATION	\
@STRIP_END@

LDADDS = @STRIP_BEGIN@ 		\
	@GLIB_LIBS@		\
@STRIP_END@

if HAVE_WINTAB
noinst_DATA = .libs/libwntab32x.a

.libs/libwntab32x.a: @WINTAB_LIB@
	cp $< $@
endif

noinst_LTLIBRARIES = libgdk-win32.la

SUBDIRS=rc

EXTRA_DIST = \
	makefile.mingw \
	makefile.msc \
	libwntab32x.la

libgdk_win32_la_SOURCES = \
	bdfcursor.c \
	xcursors.h \
	gdkcc-win32.c \
	gdkcolor-win32.c \
	gdkcursor-win32.c \
	gdkdnd-win32.c \
	gdkdrawable-win32.c \
	gdkdrawable-win32.h \
	gdkevents-win32.c \
	gdkfont-win32.c \
	gdkgc-win32.c \
	gdkglobals-win32.c \
	gdkim-win32.c \
	gdkimage-win32.c \
	gdkinput-win32.c \
	gdkinput-win32.h \
	gdkmain-win32.c \
	gdkpixmap-win32.c \
	gdkpixmap-win32.h \
	gdkprivate-win32.h \
	gdkproperty-win32.c \
	gdkregion-win32.c \
	gdkselection-win32.c \
	gdkvisual-win32.c \
	gdkwin32.h \
	gdkwin32id.c \
	gdkwindow-win32.c \
	gdkwindow-win32.h \
	surrogate-dimm.h

libgdk_win32_la_LIBADD = rc/gdk-win32res.lo
