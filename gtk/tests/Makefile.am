include $(top_srcdir)/Makefile.decl

INCLUDES =                                      \
	   -I$(top_srcdir)                      \
	   -I$(top_builddir)/gdk                \
	   -I$(top_srcdir)/gdk                  \
	   -DGDK_PIXBUF_DISABLE_DEPRECATED	\
	   -DGDK_DISABLE_DEPRECATED             \
	   -DGTK_DISABLE_DEPRECATED             \
	   $(GTK_DEBUG_FLAGS)                   \
	   $(GTK_DEP_CFLAGS)

DEPS =	$(top_builddir)/gdk-pixbuf/libgdk_pixbuf-$(GTK_API_VERSION).la  \
        $(top_builddir)/gdk/$(gdktargetlib)                             \
        $(top_builddir)/gtk/$(gtktargetlib)

progs_ldadd     = $(top_builddir)/gdk-pixbuf/libgdk_pixbuf-$(GTK_API_VERSION).la \
		  $(top_builddir)/gdk/$(gdktargetlib)                            \
		  $(top_builddir)/gtk/$(gtktargetlib)

noinst_PROGRAMS = $(TEST_PROGS)


TEST_PROGS       += testing
testing_SOURCES	  = testing.c
testing_LDADD	  = $(progs_ldadd)
