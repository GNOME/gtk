## Process this file with automake to produce Makefile.in

lib_LTLIBRARIES = libglib.la

libglib_la_SOURCES = \
		garray.c	\
		gcache.c	\
		gerror.c	\
		ghash.c		\
		glist.c		\
		gmem.c		\
		gprimes.c	\
		gslist.c	\
		gtimer.c	\
		gtree.c		\
		gutils.c        \
		gstring.c

include_HEADERS = \
		glib.h \
		glibconfig.h

libglib_la_LDFLAGS = -version-info 1:0:0

INCLUDES =

noinst_PROGRAMS = testglib
testglib_LDADD = $(top_builddir)/libglib.la

.PHONY: files release

files:
	@files=`ls $(DISTFILES) 2> /dev/null `; for p in $$files; do \
	  echo $$p; \
	done

release:
	$(MAKE) dist distdir=$(PACKAGE)`date +"%y%m%d"`
