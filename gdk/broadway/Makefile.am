## Process this file with automake to produce Makefile.in
include $(top_srcdir)/Makefile.decl
include gdk-broadway-sources.inc
NULL =

libgdkincludedir = $(includedir)/gtk-3.0/gdk
libgdkbroadwayincludedir = $(includedir)/gtk-3.0/gdk/broadway

AM_CPPFLAGS =				\
	-DG_LOG_DOMAIN=\"Gdk\"		\
	-DG_LOG_USE_STRUCTURED=1	\
	-DGDK_COMPILATION		\
	-DGDK_DISABLE_DEPRECATION_WARNINGS \
	-I$(top_srcdir)			\
	-I$(top_srcdir)/gdk		\
	-I$(top_builddir)/gdk		\
	$(GDK_HIDDEN_VISIBILITY_CFLAGS) \
	$(GTK_DEBUG_FLAGS)		\
	$(GDK_DEP_CFLAGS)

LDADDS = $(GDK_DEP_LIBS) -lrt

noinst_LTLIBRARIES = libgdk-broadway.la

bin_PROGRAMS = broadwayd

libgdkinclude_HEADERS = $(GDK_PUBLIC_H_SRCS_BROADWAY)

EXTRA_DIST += toarray.pl

clienthtml.h: client.html
	$(AM_V_GEN) $(PERL) $(srcdir)/toarray.pl client_html $(srcdir)/client.html > $@

EXTRA_DIST += client.html

broadwayjs.h: broadway.js rawinflate.min.js
	$(AM_V_GEN) $(PERL) $(srcdir)/toarray.pl broadway_js $(srcdir)/broadway.js $(srcdir)/rawinflate.min.js  > $@

EXTRA_DIST += broadway.js rawinflate.min.js

# built headers that don't get installed
broadway_built_sources =		\
	${broadway_built_private_headers}

libgdk_broadway_la_SOURCES =		\
	${broadway_built_private_headers}\
	$(GDK_BROADWAY_NON_GENERATED_SOURCES)

if OS_UNIX
libgdk_broadway_la_LIBADD = \
	$(SHM_LIBS)         \
	$(NULL)
endif

if OS_WIN32
broadwayd_LDADD = $(GDK_DEP_LIBS) -lws2_32
else
broadwayd_LDADD = $(GDK_DEP_LIBS) @SHM_LIBS@
endif

MAINTAINERCLEANFILES = $(broadway_built_sources)
EXTRA_DIST += $(broadway_built_sources)

BUILT_SOURCES = $(broadway_built_sources)

EXTRA_DIST += \
	meson.build \
	gen-c-array.py

# ------------------- MSVC Build Items ----------------
MSVCPROJS = gdk3-broadway broadwayd

gdk3_broadway_FILES = $(libgdk_broadway_la_SOURCES)
gdk3_broadway_EXCLUDES = dummy

broadwayd_FILES = $(broadwayd_SOURCES)
broadwayd_EXCLUDES = dummy

include $(top_srcdir)/win32/Makefile.msvcproj

dist-hook: \
	$(top_builddir)/win32/vs9/gdk3-broadway.vcproj		\
	$(top_builddir)/win32/vs9/broadwayd.vcproj

-include $(top_srcdir)/git.mk
