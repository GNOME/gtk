SUBDIRS = pixops

if INSIDE_GNOME_LIBS
CANVAS_PIXBUF_LIB =
CANVAS_PIXBUF_HEADERFILES =
EXTRA_GNOME_LIBS =
else
CANVAS_PIXBUF_LIB = libgnomecanvaspixbuf.la
CANVAS_PIXBUF_HEADERFILES=gnome-canvas-pixbuf.h
EXTRA_GNOME_LIBS = $(GNOME_LIBS)
endif

lib_LTLIBRARIES = 		\
	libgdk_pixbuf.la	\
	$(CANVAS_PIXBUF_LIB)

libgnomecanvaspixbuf_la_SOURCES = gnome-canvas-pixbuf.c
libgnomecanvaspixbuf_la_LDFLAGS = $(EXTRA_GNOME_LIBS)

libexecdir = $(libdir)/gdk-pixbuf/loaders

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
libpixbuf_xpm_la_LIBADD =

#
# The GIF loader
#
libpixbuf_gif_la_SOURCES = io-gif.c
libpixbuf_gif_la_LDFLAGS = -avoid-version -module
libpixbuf_gif_la_LIBADD =

#
# The ICO loader
#
libpixbuf_ico_la_SOURCES = io-ico.c
libpixbuf_ico_la_LDFLAGS = -avoid-version -module
libpixbuf_ico_la_LIBADD =

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

#
# The PNM loader
#
libpixbuf_pnm_la_SOURCES = io-pnm.c
libpixbuf_pnm_la_LDFLAGS = -avoid-version -module
libpixbuf_pnm_la_LIBADD =

#
# The BMP loader
#
libpixbuf_bmp_la_SOURCES = io-bmp.c
libpixbuf_bmp_la_LDFLAGS = -avoid-version -module
libpixbuf_bmp_la_LIBADD =

if HAVE_PNG
PNG_LIB =	libpixbuf-png.la
endif

if HAVE_JPEG
JPEG_LIB =	libpixbuf-jpeg.la
endif

GIF_LIB = libpixbuf-gif.la

ICO_LIB = libpixbuf-ico.la

RAS_LIB = libpixbuf-ras.la

if HAVE_TIFF
TIFF_LIB =	libpixbuf-tiff.la
endif

XPM_LIB =	libpixbuf-xpm.la

PNM_LIB =	libpixbuf-pnm.la

BMP_LIB =   	libpixbuf-bmp.la

if BUILD_DYNAMIC_MODULES

libexec_LTLIBRARIES = 	\
	$(PNG_LIB)	\
	$(JPEG_LIB)	\
	$(GIF_LIB)	\
	$(ICO_LIB)	\
	$(RAS_LIB)	\
	$(XPM_LIB)	\
	$(TIFF_LIB)	\
	$(PNM_LIB)	\
	$(BMP_LIB)


extra_sources = 

else
libexec_LTLIBRARIES = 

extra_sources = $(libpixbuf_png_la_SOURCES) 	\
		$(libpixbuf_jpeg_la_SOURCES) 	\
		$(libpixbuf_xpm_la_SOURCES) 	\
		$(libpixbuf_gif_la_SOURCES) 	\
		$(libpixbuf_ico_la_SOURCES) 	\
		$(libpixbuf_ras_la_SOURCES) 	\
		$(libpixbuf_tiff_la_SOURCES) 	\
		$(libpixbuf_pnm_la_SOURCES) 	\
		$(libpixbuf_bmp_la_SOURCES)

builtin_libraries = 
endif

noinst_PROGRAMS = testpixbuf testpixbuf-drawable testanimation testpixbuf-scale

DEPS = libgdk_pixbuf.la
INCLUDES = -I$(top_srcdir) -I$(top_builddir) \
	-I$(top_srcdir)/gdk-pixbuf \
	-I$(top_builddir)/gdk-pixbuf \
	$(GLIB_CFLAGS) $(LIBART_CFLAGS) $(GTK_CFLAGS)
AM_CPPFLAGS = "-DPIXBUF_LIBDIR=\"$(libexecdir)\""

LDADDS = libgdk_pixbuf.la $(LIBART_LIBS) $(GLIB_LIBS) $(GTK_LIBS) $(STATIC_LIB_DEPS)

if INSIDE_GNOME_LIBS
testpixbuf_LDADD = $(LDADDS) $(LIBART_LIBS) -lgmodule
testpixbuf_drawable_LDADD = $(LDADDS)
testpixbuf_scale_LDADD = $(LDADDS)
testanimation_LDADD = $(LDADDS) $(LIBART_LIBS) -lgmodule
else
testpixbuf_LDADD = $(LDADDS) $(LIBART_LIBS) $(GNOME_LIBS) -lgmodule
testpixbuf_drawable_LDADD = $(LDADDS) $(GNOME_LIBS)
testpixbuf_scale_LDADD = $(LDADDS) $(GNOME_LIBS)
testanimation_LDADD = $(LDADDS) $(LIBART_LIBS) $(GNOME_LIBS) -lgmodule
endif


GDK_PIXBUF_LIBS = $(LIBART_LIBS) $(GLIB_LIBS) $(GTK_LIBS)

#
# The GdkPixBuf library
#

libgdk_pixbufincludedir = $(includedir)/gdk-pixbuf

libgdk_pixbuf_la_SOURCES = 	\
	gdk-pixbuf.c		\
	gdk-pixbuf-animation.c	\
	gdk-pixbuf-data.c	\
	gdk-pixbuf-drawable.c	\
	gdk-pixbuf-io.c		\
	gdk-pixbuf-loader.c	\
	gdk-pixbuf-render.c	\
	gdk-pixbuf-scale.c	\
	gdk-pixbuf-util.c	\
	$(extra_sources)

libgdk_pixbuf_la_LDFLAGS = -version-info 1:0:0 $(LIBART_LIBS) $(GLIB_LIBS) $(GTK_LIBS)
libgdk_pixbuf_la_LIBADD = pixops/libpixops.la 

libgdk_pixbufinclude_HEADERS =	\
	gdk-pixbuf.h		\
	gdk-pixbuf-loader.h	\
	gdk-pixbuf-features.h	\
	$(CANVAS_PIXBUF_HEADERFILES)

noinst_HEADERS = \
	gdk-pixbuf-io.h

