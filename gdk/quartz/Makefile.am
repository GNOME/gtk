include $(top_srcdir)/Makefile.decl

libgdkincludedir = $(includedir)/gtk-2.0/gdk

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
	gdkapplaunchcontext-quartz.c \
	gdkcolor-quartz.c	\
	gdkcursor-quartz.c	\
	gdkdisplay-quartz.c	\
	gdkdnd-quartz.c		\
	gdkdrawable-quartz.c	\
	gdkdrawable-quartz.h	\
	gdkevents-quartz.c	\
	gdkeventloop-quartz.c	\
	gdkfont-quartz.c	\
	gdkgc-quartz.c		\
	gdkgeometry-quartz.c	\
	gdkglobals-quartz.c	\
	gdkim-quartz.c		\
	gdkimage-quartz.c	\
	gdkinput.c		\
	gdkinputprivate.h	\
	gdkkeys-quartz.c	\
	gdkmain-quartz.c	\
	gdkpixmap-quartz.c	\
	gdkpixmap-quartz.h	\
	gdkprivate-quartz.h	\
	gdkproperty-quartz.c	\
	gdkquartz.h		\
	gdkscreen-quartz.c	\
	gdkscreen-quartz.h	\
	gdkselection-quartz.c	\
	gdkspawn-quartz.c	\
	gdktestutils-quartz.c	\
	gdkvisual-quartz.c	\
	gdkwindow-quartz.c	\
	gdkwindow-quartz.h	\
	xcursors.h

libgdkinclude_HEADERS = 	\
	gdkquartz.h


-include $(top_srcdir)/git.mk
