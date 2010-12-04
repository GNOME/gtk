## Process this file with automake to produce Makefile.in
include $(top_srcdir)/Makefile.decl

libgdkincludedir = $(includedir)/gtk-3.0/gdk

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
	gdkcursor-x11.c	   	\
	gdkdevice-core.h	\
	gdkdevice-core.c	\
	gdkdevicemanager-core.h \
	gdkdevicemanager-core.c \
	gdkdevicemanager-x11.c	\
	gdkdisplay-x11.c	\
	gdkdisplay-x11.h	\
	gdkdnd-x11.c	   	\
	gdkeventsource.c	\
	gdkeventsource.h	\
	gdkeventtranslator.c	\
	gdkeventtranslator.h	\
	gdkgeometry-x11.c  	\
	gdkglobals-x11.c   	\
	gdkim-x11.c	   	\
	gdkinput.c		\
	gdkkeys-x11.c		\
	gdkmain-x11.c	   	\
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
	xsettings-client.h	\
	xsettings-client.c	\
	xsettings-common.h	\
	xsettings-common.c

if XINPUT_XFREE
libgdk_x11_la_SOURCES += 	\
	gdkdevicemanager-xi.c 	\
	gdkdevicemanager-xi.h 	\
	gdkdevice-xi.c		\
	gdkdevice-xi.h
if XINPUT_2
libgdk_x11_la_SOURCES += 	\
	gdkdevicemanager-xi2.c 	\
	gdkdevicemanager-xi2.h 	\
	gdkdevice-xi2.c		\
	gdkdevice-xi2.h
endif
endif


libgdkinclude_HEADERS = 	\
	gdkx.h


noinst_PROGRAMS = checksettings
checksettings_LDADD = libgdk-x11.la $(GLIB_LIBS)

TESTS = checksettings

# We need to include all these C files here since the conditionals
# don't seem to be correctly expanded for the dist files.
EXTRA_DIST += 			\
	gdksettings.c

-include $(top_srcdir)/git.mk
