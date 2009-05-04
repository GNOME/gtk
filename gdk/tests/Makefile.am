include $(top_srcdir)/Makefile.decl

NULL=

# check_PROGRAMS=check-gdk-cairo
check_PROGRAMS=
TESTS=$(check_PROGRAMS)
TESTS_ENVIRONMENT=GDK_PIXBUF_MODULE_FILE=$(top_builddir)/gdk-pixbuf/gdk-pixbuf.loaders

AM_CPPFLAGS=\
	$(GDK_DEP_CFLAGS) \
	-I$(top_srcdir) \
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

CLEANFILES = \
	cairosurface.png	\
	gdksurface.png

-include $(top_srcdir)/git.mk
