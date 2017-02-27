include $(top_srcdir)/Makefile.decl

libgdkincludedir = $(includedir)/gtk-4.0/gdk
libgdkquartzincludedir = $(includedir)/gtk-4.0/gdk/quartz

AM_CPPFLAGS = 				\
	-DG_LOG_DOMAIN=\"Gdk\"		\
	-DG_LOG_USE_STRUCTURED=1	\
	-DGDK_COMPILATION		\
	-I$(top_srcdir)			\
	-I$(top_srcdir)/gdk		\
	-I$(top_builddir)/gdk		\
	$(GDK_HIDDEN_VISIBILITY_CFLAGS) \
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
	gdkdisplay-quartz.h	\
	gdkdisplaymanager-quartz.c	\
	gdkdnd-quartz.c		\
	gdkdnd-quartz.h		\
	gdkevents-quartz.c	\
	gdkeventloop-quartz.c	\
	gdkglcontext-quartz.c	\
	gdkglcontext-quartz.h	\
	gdkglobals-quartz.c	\
	gdkkeys-quartz.c	\
	gdkmonitor-quartz.c     \
	gdkmonitor-quartz.h     \
	gdkprivate-quartz.h	\
	gdkproperty-quartz.c	\
	gdkquartz.h		\
	gdkscreen-quartz.c	\
	gdkscreen-quartz.h	\
	gdkselection-quartz.c	\
	gdkutils-quartz.c	\
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
	gdkquartzmonitor.h		\
	gdkquartzscreen.h		\
	gdkquartzutils.h		\
	gdkquartzwindow.h


-include $(top_srcdir)/git.mk
