if OS_WIN32
no_undefined = -no-undefined
endif

INCLUDES = \
	-I$(top_srcdir) 				\
	-I$(top_srcdir)/gtk				\
	-I$(top_builddir)/gtk				\
	-I$(top_srcdir)/gdk				\
	-DGTK_PRINT_BACKEND_ENABLE_UNSUPPORTED		\
	$(GTK_DEP_CFLAGS)

LDADDS = \
	$(GTK_DEP_LIBS)					\
	$(top_builddir)/gtk/$(gtktargetlib)		

backenddir = $(libdir)/gtk-2.0/$(GTK_BINARY_VERSION)/printbackends

backend_LTLIBRARIES = libprintbackend-pdf.la

libprintbackend_pdf_la_SOURCES =	\
	gtkprintbackendpdf.c		\
	gtkprinterpdf.c

noinst_HEADERS =			\
	gtkprintbackendpdf.h		\
	gtkprinterpdf.h

libprintbackend_pdf_la_LDFLAGS =  -avoid-version -module $(no_undefined)
libprintbackend_pdf_la_LIBADD = $(LDADDS)
