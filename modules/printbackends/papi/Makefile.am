if OS_WIN32
no_undefined = -no-undefined
endif

INCLUDES = \
	-I$(top_srcdir) 				\
	-I$(top_srcdir)/gtk				\
	-I$(top_builddir)/gtk				\
	-I$(top_srcdir)/gdk				\
	-I$(top_builddir)/gdk				\
	-DGTK_PRINT_BACKEND_ENABLE_UNSUPPORTED		\
	$(GTK_DEP_CFLAGS)				\
	$(GTK_DEBUG_FLAGS)

LDADDS = \
	$(GTK_DEP_LIBS)					\
	$(top_builddir)/gtk/$(gtktargetlib)		

backenddir = $(libdir)/gtk-2.0/$(GTK_BINARY_VERSION)/printbackends

backend_LTLIBRARIES = libprintbackend-papi.la

libprintbackend_papi_la_SOURCES =	\
	gtkprinterpapi.c		\
	gtkprintbackendpapi.c

noinst_HEADERS =			\
	gtkprinterpapi.h		\
	gtkprintbackendpapi.h

libprintbackend_papi_la_LDFLAGS =  -avoid-version -module $(no_undefined)
libprintbackend_papi_la_LIBADD = $(LDADDS) -lpapi

-include $(top_srcdir)/git.mk
