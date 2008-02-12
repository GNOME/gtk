if PLATFORM_WIN32
no_undefined = -no-undefined
endif

lib_LTLIBRARIES=libgdk_pixbuf_xlib-2.0.la

INCLUDES = \
	-I$(top_srcdir) -I$(top_builddir) 	\
	-I$(top_srcdir)/gdk-pixbuf 		\
	-I$(top_srcdir)/contrib 		\
	-DGDK_PIXBUF_DISABLE_DEPRECATED		\
	$(GDK_PIXBUF_XLIB_DEP_CFLAGS)

libgdk_pixbuf_xlib_2_0_la_LDFLAGS =         \
	-export-dynamic						\
	$(no_undefined)						\
	-version-info $(LT_VERSION_INFO)

libgdk_pixbuf_xlib_2_0_la_LIBADD = \
        $(top_builddir)/gdk-pixbuf/libgdk_pixbuf-$(GTK_API_VERSION).la \
	$(GDK_PIXBUF_XLIB_DEP_LIBS)

libgdk_pixbuf_xlib_2_0_la_SOURCES =         \
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
