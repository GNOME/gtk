include $(top_srcdir)/Makefile.decl

NULL=

check_PROGRAMS=check-gdk-cairo
TESTS=$(check_PROGRAMS)

AM_CPPFLAGS=\
	$(GDK_DEP_CFLAGS) \
	-I$(top_builddir)/gdk \
	$(NULL)

check_gdk_cairo_SOURCES=\
	check-gdk-cairo.c \
	$(NULL)
check_gdk_cairo_LDADD=\
	$(GDK_DEP_LIBS) \
	$(top_builddir)/gdk-pixbuf/libgdk_pixbuf-$(GTK_API_VERSION).la \
	$(top_builddir)/gdk/libgdk-$(gdktarget)-$(GTK_API_VERSION).la \
	$(NULL)
