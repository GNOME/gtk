include $(top_srcdir)/Makefile.decl

if PLATFORM_WIN32
no_undefined = -no-undefined
endif

backenddir = $(libdir)/gtk-3.0/$(GTK_BINARY_VERSION)/printbackends

backend_LTLIBRARIES = libprintbackend-file.la

libprintbackend_file_la_SOURCES = \
	gtkprintbackendfile.c

libprintbackend_file_la_CPPFLAGS = \
	-I$(top_srcdir) 				\
	-I$(top_srcdir)/gtk				\
	-I$(top_builddir)/gtk				\
	-I$(top_srcdir)/gdk				\
	-I$(top_builddir)/gdk				\
	-DGTK_COMPILATION				\
	-DGTK_PRINT_BACKEND_ENABLE_UNSUPPORTED		\
	$(AM_CPPFLAGS)

libprintbackend_file_la_CFLAGS = \
	$(GTK_DEP_CFLAGS)				\
	$(AM_CFLAGS)

libprintbackend_file_la_LDFLAGS =  \
	-avoid-version -module $(no_undefined)

libprintbackend_file_la_LIBADD = \
	$(top_builddir)/gtk/libgtk-3.la	\
	$(GTK_DEP_LIBS)

noinst_HEADERS =			\
	gtkprintbackendfile.h

-include $(top_srcdir)/git.mk
