lib_LTLIBRARIES=libgdk_pixbuf_xlib-1.3.la

INCLUDES = @STRIP_BEGIN@			\
	-I$(top_srcdir) -I$(top_builddir) 	\
	-I$(top_srcdir)/gdk-pixbuf 		\
	-I$(top_srcdir)/contrib 		\
	@GDK_PIXBUF_XLIB_DEP_CFLAGS@		\
@STRIP_END@

LDADDS = @STRIP_BEGIN@ 				\
	@GDK_PIXBUF_XLIB_DEP_LIBS@		\
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

pkgconfigdir   = $(libdir)/pkgconfig
pkgconfig_DATA = gdk-pixbuf-xlib-2.0.pc

EXTRA_DIST = gdk-pixbuf-xlib-2.0.pc.in
