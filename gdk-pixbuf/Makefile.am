SUBDIRS = pixops

lib_LTLIBRARIES = 		\
	libgdk_pixbuf-1.3.la

loaderdir = $(libdir)/gtk-2.0/$(GTK_VERSION)/loaders

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

#
# The WBMP loader
#
libpixbufloader_wbmp_la_SOURCES = io-wbmp.c
libpixbufloader_wbmp_la_LDFLAGS = -version-info 1:0:0 -module

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

WBMP_LIB = libpixbufloader-wbmp.la

if BUILD_DYNAMIC_MODULES

loader_LTLIBRARIES = 	\
	$(PNG_LIB)	\
	$(JPEG_LIB)	\
	$(GIF_LIB)	\
	$(ICO_LIB)	\
	$(RAS_LIB)	\
	$(XPM_LIB)	\
	$(TIFF_LIB)	\
	$(PNM_LIB)	\
	$(BMP_LIB)	\
	$(WBMP_LIB)


extra_sources = 

else
loader_LTLIBRARIES = 

extra_sources = $(libpixbufloader_png_la_SOURCES) 	\
		$(libpixbufloader_jpeg_la_SOURCES) 	\
		$(libpixbufloader_xpm_la_SOURCES) 	\
		$(libpixbufloader_gif_la_SOURCES) 	\
		$(libpixbufloader_ico_la_SOURCES) 	\
		$(libpixbufloader_ras_la_SOURCES) 	\
		$(libpixbufloader_tiff_la_SOURCES) 	\
		$(libpixbufloader_pnm_la_SOURCES) 	\
		$(libpixbufloader_bmp_la_SOURCES)	\
		$(libpixbufloader_wbmp_la_SOURCES)

builtin_libraries = 
endif

noinst_PROGRAMS = test-gdk-pixbuf
TESTS = test-gdk-pixbuf

DEPS = libgdk_pixbuf-1.3.la
INCLUDES = -I$(top_srcdir) -I$(top_builddir) \
	-I$(top_srcdir)/gdk-pixbuf \
	-I$(top_builddir)/gdk-pixbuf \
	$(GLIB_CFLAGS)
AM_CPPFLAGS = "-DPIXBUF_LIBDIR=\"$(loaderdir)\""

LDADDS = libgdk_pixbuf-1.3.la $(GLIB_LIBS) $(STATIC_LIB_DEPS)

test_gdk_pixbuf_LDADD = $(LDADDS)

bin_PROGRAMS=make-inline-pixbuf

make_inline_pixbuf_SOURCES=make-inline-pixbuf.c

make_inline_pixbuf_LDADD = $(LDADDS)

GDK_PIXBUF_LIBS = $(GLIB_LIBS) $(INTLLIBS)

#
# The GdkPixBuf library
#

libgdk_pixbufincludedir = $(includedir)/gtk-2.0/gdk-pixbuf

libgdk_pixbuf_1_3_la_SOURCES = 	\
	gdk-pixbuf-i18n.h	\
	gdk-pixbuf.c		\
	gdk-pixbuf-animation.c	\
	gdk-pixbuf-data.c	\
	gdk-pixbuf-io.c		\
	gdk-pixbuf-scale.c	\
	gdk-pixbuf-util.c	\
	$(extra_sources)

libgdk_pixbuf_1_3_la_LDFLAGS = 				        \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)    \
	$(GLIB_LIBS) 
libgdk_pixbuf_1_3_la_LIBADD = pixops/libpixops.la

libgdk_pixbufinclude_HEADERS =	\
	gdk-pixbuf.h		\
	gdk-pixbuf-features.h

noinst_HEADERS = 		\
	gdk-pixbuf-io.h		\
	gdk-pixbuf-private.h

EXTRA_DIST =			\
	makefile.mingw		\
	makefile.mingw.in	\
	gdk_pixbuf.def		\
	pixbufloader_ico.def		\
	pixbufloader_pnm.def		\
	pixbufloader_xpm.def		\
	pixbufloader_bmp.def		\
	pixbufloader_jpeg.def		\
	pixbufloader_ras.def		\
	pixbufloader_gif.def		\
	pixbufloader_png.def		\
	pixbufloader_tiff.def		\
	pixbufloader_wbmp.def

makefile.mingw: $(top_builddir)/config.status $(top_srcdir)/gdk-pixbuf/makefile.mingw.in
	cd $(top_builddir) && CONFIG_FILES=gdk-pixbuf/$@ CONFIG_HEADERS= $(SHELL) ./config.status
