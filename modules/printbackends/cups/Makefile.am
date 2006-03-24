if OS_WIN32
no_undefined = -no-undefined
endif

INCLUDES = \
	-I$(top_srcdir) 				\
	-I$(top_srcdir)/gtk				\
	-I$(top_builddir)/gtk				\
	-I$(top_srcdir)/gdk				\
	$(CUPS_CFLAGS)					\
	-DGTK_PRINT_BACKEND_ENABLE_UNSUPPORTED		\
	$(GTK_DEP_CFLAGS)

LDADDS = \
	$(GTK_DEP_LIBS)					\
	$(top_builddir)/gtk/$(gtktargetlib)		

backenddir = $(libdir)/gtk-2.0/$(GTK_BINARY_VERSION)/printbackends

backend_LTLIBRARIES = libcups.la

libcups_la_SOURCES =	\
	gtkprintbackendcups.c		\
	gtkprintercups.c		\
	gtkcupsutils.c

noinst_HEADERS =			\
	gtkprintbackendcups.h		\
	gtkprintercups.h		\
	gtkcupsutils.h

libcups_la_LDFLAGS =  -avoid-version -module $(no_undefined)
libcups_la_LIBADD = $(LDADDS) $(CUPS_LIBS)
