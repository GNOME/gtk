AM_CPPFLAGS = \
	-DSRCDIR=\"$(abs_srcdir)\"	\
	-I$(top_srcdir)			\
	-I$(top_srcdir)/gdk		\
	-I$(top_builddir)/gdk		\
	$(GTK_DEBUG_FLAGS)		\
	$(GTK_DEP_CFLAGS)

LDADD = \
        $(top_builddir)/gtk/libgtk-3.la \
        $(top_builddir)/gdk/libgdk-3.la \
        $(GTK_DEP_LIBS)

noinst_PROGRAMS = bloatpad


bloatpad_SOURCES = bloatpad.c
nodist_bloatpad_SOURCES = bloatpad-gresources.c

resource_files = \
	bloatpad.gresources.xml		\
	gtk/menus.ui

bloatpad-gresources.c: $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $<

CLEANFILES = bloatpad-gresources.c

-include $(top_srcdir)/git.mk
