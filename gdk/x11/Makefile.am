## Process this file with automake to produce Makefile.in
include $(top_srcdir)/Makefile.decl

libgdkincludedir = $(includedir)/gtk-2.0/gdk

INCLUDES = 			\
	-DG_LOG_DOMAIN=\"Gdk\"	\
	-DGDK_COMPILATION	\
	-I$(top_srcdir)		\
	-I$(top_srcdir)/gdk	\
	-I$(top_builddir)/gdk	\
	$(GTK_DEBUG_FLAGS) 	\
	$(GDK_DEP_CFLAGS)

LDADDS = $(GDK_DEP_LIBS)

noinst_LTLIBRARIES = libgdk-x11.la

libgdk_x11_la_SOURCES =    	\
	MwmUtil.h	   	\
	gdkapplaunchcontext-x11.c \
	gdkasync.c		\
	gdkasync.h		\
	gdkcolor-x11.c	   	\
	gdkcursor-x11.c	   	\
	gdkdisplay-x11.c	\
	gdkdisplay-x11.h	\
	gdkdnd-x11.c	   	\
	gdkdrawable-x11.c  	\
	gdkdrawable-x11.h	\
	gdkevents-x11.c	   	\
	gdkfont-x11.c	   	\
	gdkgc-x11.c 	   	\
	gdkgeometry-x11.c  	\
	gdkglobals-x11.c   	\
	gdkim-x11.c	   	\
	gdkimage-x11.c	   	\
	gdkinput.c	   	\
	gdkkeys-x11.c		\
	gdkmain-x11.c	   	\
	gdkpixmap-x11.c	   	\
	gdkpixmap-x11.h		\
	gdkproperty-x11.c  	\
	gdkscreen-x11.c		\
	gdkscreen-x11.h		\
	gdkselection-x11.c 	\
	gdkspawn-x11.c		\
	gdktestutils-x11.c	\
	gdkvisual-x11.c	   	\
	gdkwindow-x11.c    	\
	gdkwindow-x11.h		\
	gdkxftdefaults.c	\
	gdkxid.c	   	\
	gdkx.h		   	\
	gdkprivate-x11.h   	\
	gdkinputprivate.h  	\
	xsettings-client.h	\
	xsettings-client.c	\
	xsettings-common.h	\
	xsettings-common.c

if XINPUT_XFREE
libgdk_x11_la_SOURCES += gdkinput-x11.c gdkinput-xfree.c
else
libgdk_x11_la_SOURCES += gdkinput-none.c
endif


libgdkinclude_HEADERS = 	\
	gdkx.h


noinst_PROGRAMS = checksettings
checksettings_LDADD = libgdk-x11.la $(GLIB_LIBS)

TESTS = checksettings

# We need to include all these C files here since the conditionals
# don't seem to be correctly expanded for the dist files.
EXTRA_DIST += 			\
	gdkinput-x11.c		\
	gdkinput-xfree.c	\
	gdkinput-none.c		\
	gdksettings.c

-include $(top_srcdir)/git.mk
