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
	exampleappwin.c exampleappwin.h

-include $(top_srcdir)/git.mk
