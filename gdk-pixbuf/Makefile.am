
lib_LTLIBRARIES = 		\
	libgdk-pixbuf.la	\
	libpixbuf-png.la	\
	libpixbuf-jpeg.la	\
	libpixbuf-gif.la	\
	libpixbuf-xpm.la	\
	libpixbuf-tiff.la

noinst_PROGRAMS = testpixbuf
DEPS = libgdk-pixbuf.la
LDADDS = libgdk-pixbuf.la @GNOME_LIBDIR@ @GNOMEUI_LIBS@

testpixbuf_DEPENDENCIES = $(DEPS)
testpixbuf_LDADD = $(LDADDS)
testpixbuf_CFLAGS = $(GNOME_INCLUDEDIR)

#
# The GdkPixBuf library
#
libgdk_pixbufincludedir = $(includedir)/gdk-pixbuf $(GNOME_INCLUDEDIR)

libgdk_pixbuf_la_SOURCES = 	\
	gdk-pixbuf.c		\
	gdk-pixbuf-io.c

libgdk_pixbufinclude_HEADERS =	\
	gdk-pixbuf.h

#
# The PNG plugin.
#
libpixbuf_png_la_SOURCES = 	\
	io-png.c
libpixbuf_png_la_LDFLAGS = -avoid-version
libpixbuf_png_la_LIBADD = -lpng -lz

#
# The JPEG loader
#
libpixbuf_jpeg_la_SOURCES =	\
	io-jpeg.c
libpixbuf_jpeg_la_LDFLAGS = -avoid-version
libpixbuf_jpeg_la_LIBADD = -ljpeg

#
# The XPM loader
#
libpixbuf_xpm_la_SOURCES =	\
	io-xpm.c

#
# The GIF loader
#
libpixbuf_gif_la_SOURCES =	\
	io-gif.c
libpixbuf_gif_la_LDFLAGS = -avoid-version
libpixbuf_gif_la_LIBADD= -lungif

#
# The TIFF loader
#
libpixbuf_tiff_la_SOURCES=	\
	io-tiff.c
