
lib_LTLIBRARIES = 		\
	libgdk_pixbuf.la

libexecdir = $(libdir)/gdk-pixbuf/loaders

libexec_LTLIBRARIES =		\
	libpixbuf-png.la	\
	libpixbuf-jpeg.la	\
	libpixbuf-gif.la	\
	libpixbuf-xpm.la	\
	libpixbuf-tiff.la

noinst_PROGRAMS = testpixbuf
DEPS = libgdk_pixbuf.la
INCS = $(GNOME_INCLUDEDIR)
DEFS = "-DPIXBUF_LIBDIR=\"$(libexecdir)\""
LDADDS = libgdk_pixbuf.la @GNOME_LIBDIR@ @GNOMEUI_LIBS@

testpixbuf_DEPENDENCIES = $(DEPS)
testpixbuf_LDADD = $(LDADDS)
testpixbuf_INCLUDES = $(INCS)
testpixbuf_CFLAGS = $(GNOME_INCLUDEDIR) $(DEFS)

#
# The GdkPixBuf library
#
libgdk_pixbufincludedir = $(includedir)/gdk-pixbuf
libgdk_pixbuf_CFLAGS = $(GNOME_INCLUDEDIR) -I$(top_builddir) -I$(top_srcdir)

libgdk_pixbuf_la_SOURCES = 	\
	gdk-pixbuf.c		\
	gdk-pixbuf-io.c

libgdk_pixbufinclude_HEADERS =	\
	gdk-pixbuf.h

#
# The PNG plugin.
#
libpixbuf_png_la_SOURCES = io-png.c
libpixbuf_png_la_LDFLAGS = -avoid-version
libpixbuf_png_la_LIBADD = $(LIBPNG)

#
# The JPEG loader
#
libpixbuf_jpeg_la_SOURCES = io-jpeg.c
libpixbuf_jpeg_la_LDFLAGS = -avoid-version
libpixbuf_jpeg_la_LIBADD = $(LIBJPEG)

#
# The XPM loader
#
libpixbuf_xpm_la_SOURCES = io-xpm.c
libpixbuf_xpm_la_LDFLAGS = -avoid-version

#
# The GIF loader
#
libpixbuf_gif_la_SOURCES = io-gif.c
libpixbuf_gif_la_LDFLAGS = -avoid-version
libpixbuf_gif_la_LIBADD = $(LIBGIF)

#
# The TIFF loader
#
libpixbuf_tiff_la_SOURCES = io-tiff.c
libpixbuf_tiff_la_LDFLAGS = -avoid-version
libpixbuf_tiff_la_LIBADD = $(LIBTIFF)
