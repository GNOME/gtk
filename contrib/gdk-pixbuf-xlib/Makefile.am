
lib_LTLIBRARIES=libgdk_pixbuf_xlib-1.3.la

INCLUDES = -I$(top_srcdir) -I$(top_builddir) 	\
	-I$(top_srcdir)/gdk-pixbuf 		\
	-I$(top_srcdir)/contrib 		\
	$(GLIB_CFLAGS)				\
	@x_cflags@ 
LDADDS = @STRIP_BEGIN@ 	\
	@x_ldflags@	\
	@x_libs@	\
	@GLIB_LIBS@	\
	-lm		\
@STRIP_END@

libgdk_pixbuf_xlib_1_3_la_LDFLAGS =         \
	-export-dynamic

libgdk_pixbuf_xlib_1_3_la_SOURCES =         \
	gdk-pixbuf-xlib-private.h	\
        gdk-pixbuf-xlib.c               \
        gdk-pixbuf-xlib-render.c        \
        gdk-pixbuf-xlib-drawable.c      \
        gdk-pixbuf-xlibrgb.c

libgdk_pixbuf_xlibincludedir=$(includedir)/gtk-2.0/gdk-pixbuf-xlib

libgdk_pixbuf_xlibinclude_HEADERS =         		\
        gdk-pixbuf-xlib.h               	    	\
        gdk-pixbuf-xlibrgb.h


