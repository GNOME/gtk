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
		gstring.c	\
		gscanner.c	\
		gutils.c

include_HEADERS = \
		glib.h

libglib_la_LDFLAGS = -version-info 1:0:0

INCLUDES = -I$(top_srcdir)

BUILT_SOURCES = glib.h

glib.h: glibconfig.h glib_pre1.h glib_pre2.h
	$(srcdir)/makeglib_h $(srcdir)/glib_pre1.h \
	$(top_builddir)/glibconfig.h \
	$(srcdir)/glib_pre2.h > $@

EXTRA_DIST = \
		glib_pre1.h \
		glib_pre2.h \
		makeglib_h

noinst_PROGRAMS = testglib
testglib_LDADD = $(top_builddir)/libglib.la

.PHONY: files release

files:
	@files=`ls $(DISTFILES) 2> /dev/null `; for p in $$files; do \
	  echo $$p; \
	done

release:
	$(MAKE) dist distdir=$(PACKAGE)`date +"%y%m%d"`

$(DEP_FILES) : $(BUILT_SOURCES)
