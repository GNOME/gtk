GTK_CFLAGS = 					\
	-I$(top_srcdir)				\
	$(GTK_DEP_CFLAGS)

GTK_LIBS = 					\
        $(top_builddir)/gtk/libgtk-3.la 	\
        $(top_builddir)/gdk/libgdk-3.la 	\
        $(GTK_DEP_LIBS)


noinst_PROGRAMS = exampleapp

exampleapp_CFLAGS = $(GTK_CFLAGS)
exampleapp_LDADD = $(GTK_LIBS)
exampleapp_SOURCES = 				\
	main.c 					\
	exampleapp.c exampleapp.h 		\
	exampleappwin.c exampleappwin.h 	\
	resources.c

BUILT_SOURCES = 				\
	resources.c 				\
	gschemas.compiled

resources.c: exampleapp.gresource.xml window.ui app-menu.ui
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/exampleapp.gresource.xml \
		--target=$@ --sourcedir=$(srcdir) --generate-source

gsettings_SCHEMAS = 				\
	org.gtk.exampleapp.gschema.xml

@GSETTINGS_RULES@

gschemas.compiled: org.gtk.exampleapp.gschema.xml
	$(GLIB_COMPILE_SCHEMAS) .

EXTRA_DIST = 					\
	window.ui 				\
	app-menu.ui 				\
	exampleapp.gresource.xml 		\
	org.gtk.exampleapp.gschema.xml

CLEANFILES = 					\
	gschemas.compiled

-include $(top_srcdir)/git.mk
