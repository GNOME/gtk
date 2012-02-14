include $(top_srcdir)/Makefile.decl

NULL=

noinst_PROGRAMS = $(TEST_PROGS)

AM_CPPFLAGS = \
	$(GDK_DEP_CFLAGS) \
	-I$(top_srcdir) \
	-I$(top_builddir)/gdk \
	$(NULL)

progs_ldadd = \
	$(GDK_DEP_LIBS) \
	$(top_builddir)/gdk/libgdk-3.la \
	$(NULL)

#TEST_PROGS              += check-gdk-cairo
#check_gdk_cairo_SOURCES  = check-gdk-cairo.c
#check_gdk_cairo_LDADD    = $(progs_ldadd)

TEST_PROGS   += rgba
rgba_SOURCES  = rgba.c
rgba_LDADD    = $(progs_ldadd)

TEST_PROGS        += encoding
encoding_SOURCES   = encoding.c
encoding_LDADD     = $(progs_ldadd)

TEST_PROGS        += display
display_SOURCES    = display.c
display_LDADD      = $(progs_ldadd)

CLEANFILES = \
	cairosurface.png	\
	gdksurface.png

-include $(top_srcdir)/git.mk
