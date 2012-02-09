include $(top_srcdir)/Makefile.decl

bin_PROGRAMS = gtk3-widget-factory

gtk3_widget_factory_SOURCES = \
	widget-factory.c

gtk3_widget_factory_DEPENDENCIES = \
	$(top_builddir)/gtk/libgtk-3.la

gtk3_widget_factory_CPPFLAGS = \
	-I$(top_srcdir)			\
	$(GTK_DEBUG_FLAGS)		\
	$(GTK_DEP_CFLAGS)

gtk3_widget_factory_LDADD = \
	$(top_builddir)/gdk/libgdk-3.la \
	$(top_builddir)/gtk/libgtk-3.la

EXTRA_DIST += widget-factory.ui
