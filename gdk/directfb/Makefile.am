## Makefile.am for gtk+/gdk/gdk-directfb
include $(top_srcdir)/Makefile.decl

libgdkincludedir = $(includedir)/gtk-2.0/gdk

INCLUDES = 		\
	-DG_LOG_DOMAIN=\"Gdk-DirectFB\"	\
	-DGDK_COMPILATION \
	-I$(top_srcdir)			\
	-I$(top_srcdir)/gdk		\
	-I$(top_builddir)/gdk		\
	-DG_DISABLE_DEPRECATED		\
	@GTK_DEBUG_FLAGS@		\
	@GDK_DEP_CFLAGS@		

LDADDS = 		\
	@GDK_DEP_LIBS@		

noinst_LTLIBRARIES = libgdk-directfb.la

libgdk_directfb_la_SOURCES =	\
	gdkapplaunchcontext-directfb.c \
	gdkcolor-directfb.c	\
	gdkcursor-directfb.c	\
	gdkdnd-directfb.c	\
	gdkdisplay-directfb.c   \
	gdkdisplay-directfb.h   \
	gdkdrawable-directfb.c	\
	gdkevents-directfb.c	\
	gdkfont-directfb.c	\
	gdkgc-directfb.c	\
	gdkgeometry-directfb.c	\
	gdkglobals-directfb.c	\
	gdkim-directfb.c	\
	gdkimage-directfb.c	\
	gdkinput-directfb.c	\
	gdkinput-directfb.h	\
	gdkkeys-directfb.c	\
	gdkmain-directfb.c	\
	gdkpixmap-directfb.c	\
	gdkprivate-directfb.h	\
	gdkproperty-directfb.c	\
	gdkscreen-directfb.c	\
	gdkselection-directfb.c	\
	gdkspawn-directfb.c 	\
	gdktestutils-directfb.c	\
	gdkvisual-directfb.c	\
	gdkdirectfb.h		\
	gdkwindow-directfb.c	\
	gdkwindowid.c		\
	x-cursors.xbm

libgdkinclude_HEADERS =		\
	gdkdirectfb.h 		\
	gdkprivate-directfb.h


EXTRA_DIST += AUTHORS README TODO 

-include $(top_srcdir)/git.mk
