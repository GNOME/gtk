include $(top_srcdir)/Makefile.decl

SUBDIRS = parser

check_PROGRAMS = $(TEST_PROGS)

AM_CPPFLAGS =				\
	-I$(top_srcdir)                 \
	-I$(top_builddir)/gdk           \
	-I$(top_srcdir)/gdk             \
	-DGDK_DISABLE_DEPRECATED        \
	-DGTK_DISABLE_DEPRECATED        \
	-DGTK_VERSION=\"$(GTK_VERSION)\"\
	$(GTK_DEBUG_FLAGS)              \
	$(GTK_DEP_CFLAGS)

LDADD = \
	$(top_builddir)/gdk/libgdk-3.la \
	$(top_builddir)/gtk/libgtk-3.la \
	$(GTK_DEP_LIBS)

TEST_PROGS += api 

-include $(top_srcdir)/git.mk
