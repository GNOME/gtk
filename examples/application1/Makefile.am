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
	exampleappwin.c exampleappwin.h

-include $(top_srcdir)/git.mk
