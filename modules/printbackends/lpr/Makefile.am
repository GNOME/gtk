include $(top_srcdir)/Makefile.decl

if PLATFORM_WIN32
no_undefined = -no-undefined
endif

AM_CPPFLAGS = \
	-I$(top_srcdir) 				\
	-I$(top_srcdir)/gtk				\
	-I$(top_builddir)/gtk				\
	-I$(top_srcdir)/gdk				\
	-I$(top_builddir)/gdk				\
	-DGTK_PRINT_BACKEND_ENABLE_UNSUPPORTED		\
	$(GTK_DEP_CFLAGS)				\
	$(GTK_DEBUG_FLAGS)

LDADDS = \
	$(top_builddir)/gtk/libgtk-3.la	\
	$(GTK_DEP_LIBS)

backenddir = $(libdir)/gtk-3.0/$(GTK_BINARY_VERSION)/printbackends

backend_LTLIBRARIES = libprintbackend-lpr.la

libprintbackend_lpr_la_SOURCES =	\
	gtkprintbackendlpr.c

noinst_HEADERS =			\
	gtkprintbackendlpr.h

libprintbackend_lpr_la_LDFLAGS =  -avoid-version -module $(no_undefined)
libprintbackend_lpr_la_LIBADD = $(LDADDS)

-include $(top_srcdir)/git.mk
