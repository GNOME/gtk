lib_LTLIBRARIES = 		\
	libgdk_pixbuf.la

libexecdir = $(libdir)/gdk-pixbuf/loaders

if HAVE_PNG
PNG_LIB =	libpixbuf-png.la
endif

if HAVE_JPEG
JPEG_LIB =	libpixbuf-jpeg.la
endif

GIF_LIB = libpixbuf-gif.la

RAS_LIB = libpixbuf-ras.la

if HAVE_TIFF
TIFF_LIB =	libpixbuf-tiff.la
endif

XPM_LIB =	libpixbuf-xpm.la

libexec_LTLIBRARIES =		\
	$(PNG_LIB)	\
	$(JPEG_LIB)	\
	$(GIF_LIB)	\
	$(RAS_LIB)	\
	$(XPM_LIB)	\
	$(TIFF_LIB)

noinst_PROGRAMS = testpixbuf


DEPS = libgdk_pixbuf.la
INCLUDES = $(GLIB_CFLAGS) $(LIBART_CFLAGS) $(GTK_CFLAGS)
AM_CPPFLAGS = "-DPIXBUF_LIBDIR=\"$(libexecdir)\""

LDADDS = libgdk_pixbuf.la $(LIBART_LIBS) $(GLIB_LIBS)

testpixbuf_LDADD = $(LDADDS) $(LIBART_LIBS) $(GNOME_LIBS) -lgmodule

#
# The GdkPixBuf library
#
libgdk_pixbufincludedir = $(includedir)/gdk-pixbuf

libgdk_pixbuf_la_SOURCES = 	\
	gdk-pixbuf.c		\
	gdk-pixbuf-data.c	\
	gdk-pixbuf-drawable.c   \
	gdk-pixbuf-io.c		\
	gdk-pixbuf-loader.c	\
	gdk-pixbuf-render.c	\
	gnome-canvas-pixbuf.c

libgdk_pixbuf_la_LDFLAGS = -version-info 1:0:0

libgdk_pixbufinclude_HEADERS =	\
	gdk-pixbuf.h		\
	gdk-pixbuf-drawable.h	\
	gdk-pixbuf-loader.h	\
	gnome-canvas-pixbuf.h

noinst_HEADERS = \
	gdk-pixbuf-io.h

#
# The PNG plugin.
#
libpixbuf_png_la_SOURCES = io-png.c
libpixbuf_png_la_LDFLAGS = -avoid-version -module
libpixbuf_png_la_LIBADD = $(LIBPNG)

#
# The JPEG loader
#
libpixbuf_jpeg_la_SOURCES = io-jpeg.c
libpixbuf_jpeg_la_LDFLAGS = -avoid-version -module
libpixbuf_jpeg_la_LIBADD = $(LIBJPEG)

#
# The XPM loader
#
libpixbuf_xpm_la_SOURCES = io-xpm.c
libpixbuf_xpm_la_LDFLAGS = -avoid-version -module

#
# The GIF loader
#
libpixbuf_gif_la_SOURCES = io-gif.c
libpixbuf_gif_la_LDFLAGS = -avoid-version -module
libpixbuf_gif_la_LIBADD =

#
# The RAS loader
#
libpixbuf_ras_la_SOURCES = io-ras.c
libpixbuf_ras_la_LDFLAGS = -avoid-version -module
libpixbuf_ras_la_LIBADD =

#
# The TIFF loader
#
libpixbuf_tiff_la_SOURCES = io-tiff.c
libpixbuf_tiff_la_LDFLAGS = -avoid-version -module
libpixbuf_tiff_la_LIBADD = $(LIBTIFF)
