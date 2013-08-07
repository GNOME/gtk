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
	resources.c

BUILT_SOURCES = resources.c

resources.c: exampleapp.gresource.xml window.ui
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/exampleapp.gresource.xml \
		--target=$@ --sourcedir=$(srcdir) --generate-source

EXTRA_DIST = \
	window.ui \
	exampleapp.gresource.xml

-include $(top_srcdir)/git.mk
