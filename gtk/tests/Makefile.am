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

TEST_PROGS       += liststore
liststore_SOURCES = liststore.c
liststore_LDADD	  = $(progs_ldadd)

TEST_PROGS       += treestore
treestore_SOURCES = treestore.c
treestore_LDADD	  = $(progs_ldadd)

TEST_PROGS		       += treeview-scrolling
treeview_scrolling_SOURCES	= treeview-scrolling.c
treeview_scrolling_LDADD	= $(progs_ldadd)
