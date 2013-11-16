AM_CPPFLAGS = \
	-I$(top_srcdir)			\
	-I$(top_srcdir)/gdk		\
	-I$(top_builddir)/gdk		\
	$(GTK_DEBUG_FLAGS)		\
	$(GTK_DEP_CFLAGS)

LDADD = \
        $(top_builddir)/gtk/libgtk-3.la \
        $(top_builddir)/gdk/libgdk-3.la \
        $(GTK_DEP_LIBS)


noinst_PROGRAMS = exampleapp

exampleapp_SOURCES = \
	main.c \
	exampleapp.c exampleapp.h \
	exampleappwin.c exampleappwin.h \
	exampleappprefs.c exampleappprefs.h \
	resources.c

BUILT_SOURCES = \
	resources.c \
	gschemas.compiled

resources.c: exampleapp.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/exampleapp.gresource.xml)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/exampleapp.gresource.xml \
		--target=$@ --sourcedir=$(srcdir) --generate-source

gsettings_SCHEMAS = \
	org.gtk.exampleapp.gschema.xml

@GSETTINGS_RULES@

gschemas.compiled: org.gtk.exampleapp.gschema.xml
	$(GLIB_COMPILE_SCHEMAS) .

EXTRA_DIST = \
	window.ui \
	app-menu.ui \
	prefs.ui \
	gears-menu.ui \
	exampleapp.gresource.xml \
	org.gtk.exampleapp.gschema.xml

CLEANFILES = \
	gschemas.compiled

-include $(top_srcdir)/git.mk
