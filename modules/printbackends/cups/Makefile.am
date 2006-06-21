if OS_WIN32
no_undefined = -no-undefined
endif

INCLUDES = \
	-I$(top_srcdir) 				\
	-I$(top_srcdir)/gtk				\
	-I$(top_builddir)/gtk				\
	-I$(top_srcdir)/gdk				\
	-I$(top_builddir)/gdk				\
	$(CUPS_CFLAGS)					\
	-DGTK_PRINT_BACKEND_ENABLE_UNSUPPORTED		\
	$(GTK_DEP_CFLAGS)				\
	$(GTK_DEBUG_FLAGS)

LDADDS = \
	$(GTK_DEP_LIBS)					\
	$(top_builddir)/gtk/$(gtktargetlib)		

backenddir = $(libdir)/gtk-2.0/$(GTK_BINARY_VERSION)/printbackends

backend_LTLIBRARIES = libprintbackend-cups.la

libprintbackend_cups_la_SOURCES =	\
	gtkprintbackendcups.c		\
	gtkprintercups.c		\
	gtkcupsutils.c

noinst_HEADERS =			\
	gtkprintbackendcups.h		\
	gtkprintercups.h		\
	gtkcupsutils.h

libprintbackend_cups_la_LDFLAGS =  -avoid-version -module $(no_undefined)
libprintbackend_cups_la_LIBADD = $(LDADDS) $(CUPS_LIBS)
