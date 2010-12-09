## Process this file with automake to produce Makefile.in
include $(top_srcdir)/Makefile.decl

libgdkincludedir = $(includedir)/gtk-3.0/gdk

INCLUDES = \
	-DG_LOG_DOMAIN=\"Gdk\"	\
	-DINSIDE_GDK_WIN32	\
	-I$(top_srcdir)		\
	-I$(top_srcdir)/gdk	\
	-I$(top_builddir)/gdk	\
	$(GTK_DEBUG_FLAGS) 	\
	$(GDK_DEP_CFLAGS)	\
	$(GDK_WIN32_EXTRA_CFLAGS)\
	-DGDK_COMPILATION

LDADDS = $(GDK_DEP_LIBS)

noinst_LTLIBRARIES = libgdk-win32.la

SUBDIRS=rc

EXTRA_DIST += \
	bdfcursor.c \
	makefile.msc

libgdk_win32_la_SOURCES = \
	xcursors.h \
	gdkcursor-win32.c \
	gdkdevicemanager-win32.c \
	gdkdevicemanager-win32.h \
	gdkdevice-win32.c \
	gdkdevice-win32.h \
	gdkdevice-wintab.c \
	gdkdevice-wintab.h \
	gdkdisplay-win32.c \
	gdkdnd-win32.c \
	gdkdrawable-win32.c \
	gdkdrawable-win32.h \
	gdkevents-win32.c \
	gdkgeometry-win32.c \
	gdkglobals-win32.c \
	gdkim-win32.c \
	gdkinput.c \
	gdkkeys-win32.c \
	gdkmain-win32.c \
	gdkprivate-win32.h \
	gdkproperty-win32.c \
	gdkscreen-win32.c \
	gdkselection-win32.c \
	gdkspawn-win32.c \
	gdktestutils-win32.c \
	gdkvisual-win32.c \
	gdkwin32.h \
	gdkwin32id.c \
	gdkwindow-win32.c \
	gdkwindow-win32.h \
	wintab.h \
	pktdef.h

libgdkinclude_HEADERS =		\
	gdkwin32.h

-include $(top_srcdir)/git.mk
