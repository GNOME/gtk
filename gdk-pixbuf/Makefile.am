common_ldflags = -aviod-version
noinst_PROGRAMS = testpixbuf
loader_dir=$(libdir)/gdk-pixbuf/loaders
DEFS = -DPIXBUF_LIBDIR="$(loader_dir)"
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
libpixbuf_png_la_SOURCES = io-png.c
libpixbuf_png_la_LDFLAGS = $(common_ldflags)
libpixbuf_png_la_LIBADD = $(PNGLIBS)

#
# The JPEG loader
#
libpixbuf_jpeg_la_SOURCES = io-jpeg.c
libpixbuf_jpeg_la_LDFLAGS = $(common_ldflags)
libpixbuf_jpeg_la_LIBADD = $(JPEGLIBS)

#
# The XPM loader
#
libpixbuf_xpm_la_SOURCES = io-xpm.c
libpixbuf_xpm_la_LDFLAGS = $(common_ldflags)
libpixbuf_xpm_la_LIBADD =


#
# The GIF loader
#
libpixbuf_gif_la_SOURCES = io-gif.c
libpixbuf_gif_la_LDFLAGS = $(common_ldflags)
libpixbuf_gif_la_LIBADD= $(GIFLIB)

#
# The TIFF loader
#
libpixbuf_tiff_la_SOURCES = io-tiff.c
libpixbuf_tiff_la_LDFLAGS = $(common_ldflags)
libpixbuf_tiff_la_LIBADD = $(TIFFLIB)

lib_LTLIBRARIES =               \
        libgdk-pixbuf.la 

libpixbufloader_DATA =		\
	libpixbuf-tiff.la	\
	libpixbuf-gif.la	\
	libpixbuf-xpm.la	\
	libpixbuf-jpeg.la	\
	libpixbuf-png.la
