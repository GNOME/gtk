AM_CPPFLAGS =				\
	-I$(top_srcdir)			\
	-I$(top_builddir)/gdk		\
	-I$(top_srcdir)/gdk		\
	$(GTK_DEBUG_FLAGS)		\
	$(GTK_DEP_CFLAGS)		\
	$(GDK_DEP_CFLAGS)

AM_CXXFLAGS = $(AM_CPPFLAGS)

DEPS = \
	$(top_builddir)/gtk/libgtk-3.la

LDADDS = \
	$(top_builddir)/gtk/libgtk-3.la	\
	$(top_builddir)/gdk/libgdk-3.la	\
	$(GTK_DEP_LIBS)			\
	-lm

noinst_PROGRAMS =  visuals

visuals_DEPENDENCIES = $(TEST_DEPS)
visuals_LDADD = $(LDADDS)
visuals_SOURCES = visuals.c

EXTRA_DIST = \
	inline-toolbar-horizontal.ui \
	inline-toolbar-vertical.ui \
	level-bar.ui \
	linked-buttons-horizontal.ui \
	linked-buttons-vertical.ui \
	osd-toolbars.ui \
	primary-toolbar.ui \
	suggested-action-buttons.ui

-include $(top_srcdir)/git.mk
