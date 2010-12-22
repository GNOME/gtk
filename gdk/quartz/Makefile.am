include $(top_srcdir)/Makefile.decl

libgdkincludedir = $(includedir)/gtk-3.0/gdk

INCLUDES = 				\
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
	GdkQuartzWindow.c	\
	GdkQuartzWindow.h	\
	gdkcursor-quartz.c	\
	gdkcursor-quartz.h	\
	gdkdevice-core.c	\
	gdkdevicemanager-core.c	\
	gdkdisplay-quartz.c	\
	gdkdisplay-quartz.h	\
	gdkdisplaymanager-quartz.c	\
	gdkdisplaymanager-quartz.h	\
	gdkdnd-quartz.c		\
	gdkdnd-quartz.h		\
	gdkevents-quartz.c	\
	gdkeventloop-quartz.c	\
	gdkgeometry-quartz.c	\
	gdkglobals-quartz.c	\
	gdkkeys-quartz.c	\
	gdkmain-quartz.c	\
	gdkprivate-quartz.h	\
	gdkproperty-quartz.c	\
	gdkquartz.h		\
	gdkscreen-quartz.c	\
	gdkscreen-quartz.h	\
	gdkselection-quartz.c	\
	gdktestutils-quartz.c	\
	gdkvisual-quartz.c	\
	gdkwindow-quartz.c	\
	gdkwindow-quartz.h	\
	xcursors.h

libgdkinclude_HEADERS = 	\
	gdkquartz.h


-include $(top_srcdir)/git.mk
