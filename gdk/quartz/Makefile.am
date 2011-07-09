include $(top_srcdir)/Makefile.decl

libgdkincludedir = $(includedir)/gtk-3.0/gdk
libgdkquartzincludedir = $(includedir)/gtk-3.0/gdk/quartz

AM_CPPFLAGS = 				\
	-DG_LOG_DOMAIN=\"Gdk\"		\
	-DGDK_COMPILATION		\
	-I$(top_srcdir)			\
	-I$(top_srcdir)/gdk		\
	-I$(top_builddir)/gdk		\
	$(GTK_DEBUG_FLAGS) 		\
	$(GDK_DEP_CFLAGS)		\
	"-xobjective-c"

LDADDS = $(GDK_DEP_LIBS)

noinst_LTLIBRARIES = libgdk-quartz.la

libgdk_quartz_la_SOURCES =    	\
	GdkQuartzView.c		\
	GdkQuartzView.h		\
	GdkQuartzNSWindow.c	\
	GdkQuartzNSWindow.h	\
	gdkcursor-quartz.c	\
	gdkdevice-core-quartz.c	\
	gdkdevicemanager-core-quartz.c	\
	gdkdevicemanager-core-quartz.h	\
	gdkdisplay-quartz.c	\
	gdkdisplaymanager-quartz.c	\
	gdkdnd-quartz.c		\
	gdkdnd-quartz.h		\
	gdkevents-quartz.c	\
	gdkeventloop-quartz.c	\
	gdkglobals-quartz.c	\
	gdkkeys-quartz.c	\
	gdkprivate-quartz.h	\
	gdkproperty-quartz.c	\
	gdkquartz.h		\
	gdkscreen-quartz.c	\
	gdkscreen-quartz.h	\
	gdkselection-quartz.c	\
	gdktestutils-quartz.c	\
	gdkutils-quartz.c	\
	gdkvisual-quartz.c	\
	gdkwindow-quartz.c	\
	gdkwindow-quartz.h	\
	xcursors.h

libgdkinclude_HEADERS = 	\
	gdkquartz.h

libgdkquartzinclude_HEADERS =		\
	gdkquartzcursor.h		\
	gdkquartzdevice-core.h		\
	gdkquartzdevicemanager-core.h	\
	gdkquartzdisplay.h		\
	gdkquartzdisplaymanager.h	\
	gdkquartzdnd.h			\
	gdkquartzkeys.h			\
	gdkquartzscreen.h		\
	gdkquartzutils.h		\
	gdkquartzvisual.h		\
	gdkquartzwindow.h


-include $(top_srcdir)/git.mk
