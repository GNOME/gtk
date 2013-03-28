## Process this file with automake to produce Makefile.in
include $(top_srcdir)/Makefile.decl

libgdkincludedir = $(includedir)/gtk-3.0/gdk
libgdkbroadwayincludedir = $(includedir)/gtk-3.0/gdk/broadway

AM_CPPFLAGS =			\
	-DG_LOG_DOMAIN=\"Gdk\"	\
	-DGDK_COMPILATION	\
	-I$(top_srcdir)		\
	-I$(top_srcdir)/gdk	\
	-I$(top_builddir)/gdk	\
	$(GTK_DEBUG_FLAGS)	\
	$(GDK_DEP_CFLAGS)

LDADDS = $(GDK_DEP_LIBS)

noinst_LTLIBRARIES = libgdk-broadway.la

bin_PROGRAMS = broadwayd

libgdkinclude_HEADERS = 	\
	gdkbroadway.h

libgdkbroadwayinclude_HEADERS = 	\
	gdkbroadwaydisplaymanager.h	\
	gdkbroadwaywindow.h		\
	gdkbroadwaycursor.h		\
	gdkbroadwayvisual.h

clienthtml.h: client.html
	$(PERL) $(srcdir)/toarray.pl $(srcdir)/client.html client_html > $@

EXTRA_DIST += client.html

broadwayjs.h: broadway.js
	$(PERL) $(srcdir)/toarray.pl $(srcdir)/broadway.js broadway_js > $@

EXTRA_DIST += broadway.js

# built headers that don't get installed
broadway_built_private_headers =	\
	clienthtml.h			\
	broadwayjs.h

broadway_built_sources =		\
	${broadway_built_private_headers}

libgdk_broadway_la_SOURCES =		\
	${broadway_built_private_headers}\
	gdkcursor-broadway.c	\
	gdkdevice-broadway.h	\
	gdkdevice-broadway.c	\
	gdkdevicemanager-broadway.h \
	gdkdevicemanager-broadway.c \
	gdkdisplay-broadway.c	\
	gdkdisplay-broadway.h	\
	gdkdisplaymanager-broadway.c	\
	gdkdnd-broadway.c	\
	gdkeventsource.c	\
	gdkeventsource.h	\
	gdkglobals-broadway.c	\
	gdkim-broadway.c	\
	gdkkeys-broadway.c	\
	gdkmain-broadway.c	\
	gdkproperty-broadway.c	\
	gdkscreen-broadway.c	\
	gdkscreen-broadway.h	\
	gdkselection-broadway.c	\
	gdktestutils-broadway.c	\
	gdkvisual-broadway.c	\
	gdkwindow-broadway.c	\
	gdkwindow-broadway.h	\
	gdkprivate-broadway.h   \
	gdkbroadway-server.h	\
	gdkbroadway-server.c

broadwayd_SOURCES = \
	broadway-protocol.h		\
	broadwayd.c 			\
	broadway-server.h		\
	broadway-server.c		\
	broadway-output.h		\
	broadway-output.c

broadwayd_LDADD = $(GDK_DEP_LIBS) -lrt -lcrypt

MAINTAINERCLEANFILES = $(broadway_built_sources)
EXTRA_DIST += $(broadway_built_sources)

BUILT_SOURCES = $(broadway_built_sources)

-include $(top_srcdir)/git.mk
