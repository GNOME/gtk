## Process this file with automake to produce Makefile.in
include $(top_srcdir)/Makefile.decl

libgdkincludedir = $(includedir)/gtk-3.0/gdk

INCLUDES =			\
	-DG_LOG_DOMAIN=\"Gdk\"	\
	-DGDK_COMPILATION	\
	-I$(top_srcdir)		\
	-I$(top_srcdir)/gdk	\
	-I$(top_builddir)/gdk	\
	$(GTK_DEBUG_FLAGS)	\
	$(GDK_DEP_CFLAGS)

LDADDS = $(GDK_DEP_LIBS)

noinst_LTLIBRARIES = libgdk-broadway.la

libgdk_broadway_la_SOURCES =		\
	MwmUtil.h		\
	gdkapplaunchcontext-broadway.c \
	gdkasync.c		\
	gdkasync.h		\
	gdkcursor-broadway.c	\
	gdkdevice-broadway.h	\
	gdkdevice-broadway.c	\
	gdkdevicemanager-broadway.h \
	gdkdevicemanager-broadway.c \
	gdkdisplay-broadway.c	\
	gdkdisplay-broadway.h	\
	gdkdnd-broadway.c	\
	gdkdrawable-broadway.c	\
	gdkdrawable-broadway.h	\
	gdkeventsource.c	\
	gdkeventsource.h	\
	gdkgeometry-broadway.c	\
	gdkglobals-broadway.c	\
	gdkim-broadway.c	\
	gdkinput.c		\
	gdkkeys-broadway.c	\
	gdkmain-broadway.c	\
	gdkproperty-broadway.c	\
	gdkscreen-broadway.c	\
	gdkscreen-broadway.h	\
	gdkselection-broadway.c	\
	gdkspawn-broadway.c	\
	gdktestutils-broadway.c	\
	gdkvisual-broadway.c	\
	gdkwindow-broadway.c	\
	gdkwindow-broadway.h	\
	gdkxftdefaults.c	\
	gdkxid.c		\
	gdkx.h			\
	gdkprivate-broadway.h	\
	xsettings-client.h	\
	xsettings-client.c	\
	xsettings-common.h	\
	xsettings-common.c

libgdkinclude_HEADERS =		\
	gdkx.h


# We need to include all these C files here since the conditionals
# don't seem to be correctly expanded for the dist files.
EXTRA_DIST +=			\
	gdksettings.c

-include $(top_srcdir)/git.mk
