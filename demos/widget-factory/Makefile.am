include $(top_srcdir)/Makefile.decl

bin_PROGRAMS = gtk3-widget-factory

gtk3_widget_factory_SOURCES = \
	widget-factory.c	\
	widget_factory_resources.c

BUILT_SOURCES = \
	widget_factory_resources.c

gtk3_widget_factory_DEPENDENCIES = \
	$(top_builddir)/gtk/libgtk-3.la

gtk3_widget_factory_CPPFLAGS = \
	-I$(top_srcdir)			\
	$(GTK_DEBUG_FLAGS)		\
	$(GTK_DEP_CFLAGS)

gtk3_widget_factory_LDADD = \
	$(top_builddir)/gdk/libgdk-3.la \
	$(top_builddir)/gtk/libgtk-3.la \
	$(GTK_DEP_LIBS)

widget_factory_resources.c: widget-factory.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/widget-factory.gresource.xml)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $<

EXTRA_DIST += \
	widget-factory.ui \
	widget-factory.gresource.xml \
	gtk-logo-256.png

-include $(top_srcdir)/git.mk
