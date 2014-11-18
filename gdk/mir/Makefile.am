## Process this file with automake to produce Makefile.in
include $(top_srcdir)/Makefile.decl

libgdkincludedir = $(includedir)/gtk-3.0/gdk
libgdkmirincludedir = $(includedir)/gtk-3.0/gdk/mir

AM_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"Gdk\" \
	-DGDK_COMPILATION \
	-I$(top_srcdir) \
	-I$(top_srcdir)/gdk \
	-I$(top_builddir)/gdk \
	$(GDK_HIDDEN_VISIBILITY_CFLAGS) \
	$(GTK_DEBUG_FLAGS) \
	$(GDK_DEP_CFLAGS)

LDADDS = $(GDK_DEP_LIBS)

noinst_LTLIBRARIES = \
	libgdk-mir.la

libgdk_mir_la_SOURCES =	\
	gdkmircursor.c \
	gdkmirdevicemanager.c \
	gdkmirdisplay.c \
	gdkmireventsource.c	\
	gdkmirglcontext.c \
	gdkmirkeyboard.c \
	gdkmirkeymap.c \
	gdkmirpointer.c \
	gdkmirscreen.c	\
	gdkmirwindow.c	\
	gdkmirwindowimpl.c	\
	gdkmir-debug.c	\
	gdkmir.h

libgdkinclude_HEADERS = \
	gdkmir.h

-include $(top_srcdir)/git.mk
