SUBDIRS = pixops

if BUILD_CANVAS_ITEM
CANVAS_PIXBUF_LIB = libgnomecanvaspixbuf.la
CANVAS_PIXBUF_HEADERFILES=gnome-canvas-pixbuf.h
libgnomecanvaspixbuf_la_SOURCES = gnome-canvas-pixbuf.c
EXTRA_GNOME_LIBS = $(GNOME_LIBS)
else
CANVAS_PIXBUF_LIB =
CANVAS_PIXBUF_HEADERFILES =
EXTRA_GNOME_LIBS =
endif

lib_LTLIBRARIES = 		\
	libgdk_pixbuf.la	\
	$(CANVAS_PIXBUF_LIB)

libgnomecanvaspixbuf_la_LDFLAGS = -version-info 1:0:0 $(EXTRA_GNOME_LIBS)

libexecdir = $(libdir)/gdk-pixbuf/loaders

#
# The PNG plugin.
#
libpixbufloader_png_la_SOURCES = io-png.c
libpixbufloader_png_la_LDFLAGS = -version-info 1:0:0 -module
libpixbufloader_png_la_LIBADD = $(LIBPNG)

#
# The JPEG loader
#
libpixbufloader_jpeg_la_SOURCES = io-jpeg.c
libpixbufloader_jpeg_la_LDFLAGS = -version-info 1:0:0 -module
libpixbufloader_jpeg_la_LIBADD = $(LIBJPEG)

#
# The XPM loader
#
libpixbufloader_xpm_la_SOURCES = io-xpm.c
libpixbufloader_xpm_la_LDFLAGS = -version-info 1:0:0 -module
libpixbufloader_xpm_la_LIBADD =

#
# The GIF loader
#
libpixbufloader_gif_la_SOURCES = io-gif.c
libpixbufloader_gif_la_LDFLAGS = -version-info 1:0:0 -module
libpixbufloader_gif_la_LIBADD =

#
# The ICO loader
#
libpixbufloader_ico_la_SOURCES = io-ico.c
libpixbufloader_ico_la_LDFLAGS = -version-info 1:0:0 -module
libpixbufloader_ico_la_LIBADD =

#
# The RAS loader
#
libpixbufloader_ras_la_SOURCES = io-ras.c
libpixbufloader_ras_la_LDFLAGS = -version-info 1:0:0 -module
libpixbufloader_ras_la_LIBADD =

#
# The TIFF loader
#
libpixbufloader_tiff_la_SOURCES = io-tiff.c
libpixbufloader_tiff_la_LDFLAGS = -version-info 1:0:0 -module
libpixbufloader_tiff_la_LIBADD = $(LIBTIFF)

#
# The PNM loader
#
libpixbufloader_pnm_la_SOURCES = io-pnm.c
libpixbufloader_pnm_la_LDFLAGS = -version-info 1:0:0 -module
libpixbufloader_pnm_la_LIBADD =

#
# The BMP loader
#
libpixbufloader_bmp_la_SOURCES = io-bmp.c
libpixbufloader_bmp_la_LDFLAGS = -version-info 1:0:0 -module
libpixbufloader_bmp_la_LIBADD =

if HAVE_PNG
PNG_LIB = libpixbufloader-png.la
endif

if HAVE_JPEG
JPEG_LIB = libpixbufloader-jpeg.la
endif

GIF_LIB = libpixbufloader-gif.la

ICO_LIB = libpixbufloader-ico.la

RAS_LIB = libpixbufloader-ras.la

if HAVE_TIFF
TIFF_LIB = libpixbufloader-tiff.la
endif

XPM_LIB = libpixbufloader-xpm.la

PNM_LIB = libpixbufloader-pnm.la

BMP_LIB = libpixbufloader-bmp.la

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

extra_sources = $(libpixbufloader_png_la_SOURCES) 	\
		$(libpixbufloader_jpeg_la_SOURCES) 	\
		$(libpixbufloader_xpm_la_SOURCES) 	\
		$(libpixbufloader_gif_la_SOURCES) 	\
		$(libpixbufloader_ico_la_SOURCES) 	\
		$(libpixbufloader_ras_la_SOURCES) 	\
		$(libpixbufloader_tiff_la_SOURCES) 	\
		$(libpixbufloader_pnm_la_SOURCES) 	\
		$(libpixbufloader_bmp_la_SOURCES)

builtin_libraries = 
endif

noinst_PROGRAMS = testpixbuf testpixbuf-drawable testanimation testpixbuf-scale

DEPS = libgdk_pixbuf.la
INCLUDES = -I$(top_srcdir) -I$(top_builddir) \
	-I$(top_srcdir)/gdk-pixbuf \
	-I$(top_builddir)/gdk-pixbuf \
	$(GLIB_CFLAGS) $(GTK_CFLAGS) $(GNOME_CFLAGS)
AM_CPPFLAGS = "-DPIXBUF_LIBDIR=\"$(libexecdir)\""

LDADDS = libgdk_pixbuf.la $(GLIB_LIBS) $(GTK_LIBS) $(STATIC_LIB_DEPS)

if INSIDE_GNOME_LIBS
testpixbuf_LDADD = $(LDADDS) -lgmodule
testpixbuf_drawable_LDADD = $(LDADDS)
testpixbuf_scale_LDADD = $(LDADDS)
testanimation_LDADD = $(LDADDS) -lgmodule
else
testpixbuf_LDADD = $(LDADDS) $(GNOME_LIBS) -lgmodule
testpixbuf_drawable_LDADD = $(LDADDS) $(GNOME_LIBS)
testpixbuf_scale_LDADD = $(LDADDS) $(GNOME_LIBS)
testanimation_LDADD = $(LDADDS) $(GNOME_LIBS) -lgmodule
endif


GDK_PIXBUF_LIBS = $(GLIB_LIBS) $(GTK_LIBS)

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

libgdk_pixbuf_la_LDFLAGS = -version-info 2:0:0 $(GLIB_LIBS) $(GTK_LIBS)
libgdk_pixbuf_la_LIBADD = pixops/libpixops.la

libgdk_pixbufinclude_HEADERS =	\
	gdk-pixbuf.h		\
	gdk-pixbuf-loader.h	\
	gdk-pixbuf-features.h	\
	$(CANVAS_PIXBUF_HEADERFILES)

noinst_HEADERS = 		\
	gdk-pixbuf-io.h		\
	gdk-pixbuf-private.h
