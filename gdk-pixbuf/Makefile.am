SUBDIRS = pixops

lib_LTLIBRARIES = 		\
	libgdk_pixbuf-1.3.la

loaderdir = $(libdir)/gtk-2.0/$(GTK_VERSION)/loaders

module_libs = libgdk_pixbuf-1.3.la $(GDK_PIXBUF_DEP_LIBS)

#
# The PNG plugin.
#
libpixbufloader_png_la_SOURCES = io-png.c
libpixbufloader_png_la_LDFLAGS = -avoid-version -module
libpixbufloader_png_la_LIBADD = $(LIBPNG) $(module_libs)
libpixbufloader_static_png_la_SOURCES = io-png.c

#
# The JPEG loader
#
libpixbufloader_static_jpeg_la_SOURCES = io-jpeg.c
libpixbufloader_jpeg_la_SOURCES = io-jpeg.c
libpixbufloader_jpeg_la_LDFLAGS = -avoid-version -module
libpixbufloader_jpeg_la_LIBADD = $(LIBJPEG) $(module_libs)

#
# The XPM loader
#
libpixbufloader_static_xpm_la_SOURCES = io-xpm.c
libpixbufloader_xpm_la_SOURCES = io-xpm.c
libpixbufloader_xpm_la_LDFLAGS = -avoid-version -module
libpixbufloader_xpm_la_LIBADD = $(module_libs)

#
# The GIF loader
#
libpixbufloader_static_gif_la_SOURCES = io-gif.c io-gif-animation.c io-gif-animation.h
libpixbufloader_gif_la_SOURCES = io-gif.c io-gif-animation.c io-gif-animation.h
libpixbufloader_gif_la_LDFLAGS = -avoid-version -module
libpixbufloader_gif_la_LIBADD = $(module_libs)

#
# The ICO loader
#
libpixbufloader_static_ico_la_SOURCES = io-ico.c
libpixbufloader_ico_la_SOURCES = io-ico.c
libpixbufloader_ico_la_LDFLAGS = -avoid-version -module
libpixbufloader_ico_la_LIBADD = $(module_libs)

#
# The RAS loader
#
libpixbufloader_static_ras_la_SOURCES = io-ras.c
libpixbufloader_ras_la_SOURCES = io-ras.c
libpixbufloader_ras_la_LDFLAGS = -avoid-version -module
libpixbufloader_ras_la_LIBADD = $(module_libs)

#
# The TIFF loader
#
libpixbufloader_static_tiff_la_SOURCES = io-tiff.c
libpixbufloader_tiff_la_SOURCES = io-tiff.c
libpixbufloader_tiff_la_LDFLAGS = -avoid-version -module
libpixbufloader_tiff_la_LIBADD = $(LIBTIFF) $(module_libs)

#
# The PNM loader
#
libpixbufloader_static_pnm_la_SOURCES = io-pnm.c
libpixbufloader_pnm_la_SOURCES = io-pnm.c
libpixbufloader_pnm_la_LDFLAGS = -avoid-version -module
libpixbufloader_pnm_la_LIBADD = $(module_libs)

#
# The BMP loader
#
libpixbufloader_static_bmp_la_SOURCES = io-bmp.c
libpixbufloader_bmp_la_SOURCES = io-bmp.c
libpixbufloader_bmp_la_LDFLAGS = -avoid-version -module
libpixbufloader_bmp_la_LIBADD = $(module_libs)

#
# The WBMP loader
#
libpixbufloader_static_wbmp_la_SOURCES = io-wbmp.c
libpixbufloader_wbmp_la_SOURCES = io-wbmp.c
libpixbufloader_wbmp_la_LDFLAGS = -avoid-version -module
libpixbufloader_wbmp_la_LIBADD = $(module_libs)

#
# The XBM loader
#
libpixbufloader_xbm_la_SOURCES = io-xbm.c
libpixbufloader_xbm_la_LDFLAGS = -version-info 1:0:0 -module
libpixbufloader_xbm_la_LDFLAGS = -avoid-version -module
libpixbufloader_xbm_la_LIBADD = $(module_libs)

#
# The TGA loader
#
libpixbufloader_tga_la_SOURCES = io-tga.c
libpixbufloader_tga_la_LDFLAGS = -version-info 1:0:0 -module
libpixbufloader_tga_la_LDFLAGS = -avoid-version -module
libpixbufloader_tga_la_LIBADD = $(module_libs)

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

XBM_LIB = libpixbufloader-xbm.la

TGA_LIB = libpixbufloader-tga.la

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
	$(WBMP_LIB)	\
	$(XBM_LIB)	\
	$(TGA_LIB)


extra_sources = 
builtin_objs = 
else
loader_LTLIBRARIES = 

noinst_LTLIBRARIES =  	\
	libpixbufloader-static-png.la \
	libpixbufloader-static-jpeg.la \
	libpixbufloader-static-gif.la  \
	libpixbufloader-static-ico.la  \
	libpixbufloader-static-ras.la  \
	libpixbufloader-static-tiff.la \
	libpixbufloader-static-xpm.la  \
	libpixbufloader-static-pnm.la  \
	libpixbufloader-static-bmp.la  \
	libpixbufloader-static-wbmp.la

builtin_objs = @INCLUDED_LOADER_OBJ@

endif

TESTS = test-gdk-pixbuf test-loaders
noinst_PROGRAMS = $(TESTS)

DEPS = libgdk_pixbuf-1.3.la
INCLUDES = @STRIP_BEGIN@ \
	-I$(top_srcdir) -I$(top_builddir) 	\
	-I$(top_srcdir)/gdk-pixbuf 		\
	-I$(top_builddir)/gdk-pixbuf 		\
	@INCLUDED_LOADER_DEFINE@ 		\
	@GTK_DEBUG_FLAGS@			\
	@GDK_PIXBUF_DEP_CFLAGS@			\
@STRIP_END@

AM_CPPFLAGS = "-DPIXBUF_LIBDIR=\"$(loaderdir)\"" "-DBUILT_MODULES_DIR=\"$(srcdir)/.libs\""

LDADDS = libgdk_pixbuf-1.3.la

test_gdk_pixbuf_LDADD = $(LDADDS)
test_loaders_LDADD = $(LDADDS)

test_loaders_SOURCES = test-loaders.c test-images.h

#noinst_PROGRAMS=make-inline-pixbuf
#make_inline_pixbuf_SOURCES=make-inline-pixbuf.c
#make_inline_pixbuf_LDADD = $(LDADDS)

bin_PROGRAMS = gdk-pixbuf-csource
gdk_pixbuf_csource_SOURCES = gdk-pixbuf-csource.c
gdk_pixbuf_csource_LDADD = $(LDADDS)


#
# The GdkPixBuf library
#

BUILT_SOURCES=gdk-pixbuf-marshal.h gdk-pixbuf-marshal.c

libgdk_pixbufincludedir = $(includedir)/gtk-2.0/gdk-pixbuf

libgdk_pixbuf_1_3_la_SOURCES = @STRIP_BEGIN@ \
	gdk-pixbuf-i18n.h	\
	gdk-pixbuf.c		\
	gdk-pixbuf-animation.c	\
	gdk-pixbuf-data.c	\
	gdk-pixbuf-io.c		\
	gdk-pixbuf-loader.c	\
	gdk-pixbuf-scale.c	\
	gdk-pixbuf-util.c	\
	gdk-pixdata.c		\
@STRIP_END@


$(libgdk_pixbuf_1_3_la_OBJECTS): gdk-pixbuf-marshal.h gdk-pixbuf-marshal.c

libgdk_pixbuf_1_3_la_LDFLAGS = @STRIP_BEGIN@ \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)    \
	@LIBTOOL_EXPORT_OPTIONS@				\
@STRIP_END@

libgdk_pixbuf_1_3_la_LIBADD = pixops/libpixops.la $(builtin_objs) $(GDK_PIXBUF_DEP_LIBS)
libgdk_pixbuf_1_3_la_DEPENDENCIES = $(builtin_objs)

libgdk_pixbufinclude_HEADERS =  \
	gdk-pixbuf.h		\
	gdk-pixbuf-loader.h	\
	gdk-pixbuf-features.h	\
	gdk-pixdata.h

noinst_HEADERS = 		\
	gdk-pixbuf-io.h		\
	gdk-pixbuf-private.h

EXTRA_DIST =			\
	makefile.mingw		\
	makefile.mingw.in	\
	gdk_pixbuf.def		\
	gdk-pixbuf-marshal.list		\
	pixbufloader_ico.def		\
	pixbufloader_pnm.def		\
	pixbufloader_xpm.def		\
	pixbufloader_bmp.def		\
	pixbufloader_jpeg.def		\
	pixbufloader_ras.def		\
	pixbufloader_gif.def		\
	pixbufloader_png.def		\
	pixbufloader_tiff.def		\
	pixbufloader_wbmp.def		\
	pixbufloader_xbm.def


gdk-pixbuf-marshal.h: @REBUILD@ stamp-gdk-pixbuf-marshal.h
	@true

gdk-pixbuf-marshal.c: @REBUILD@ stamp-gdk-pixbuf-marshal.c
	@true

stamp-gdk-pixbuf-marshal.h: @REBUILD@ $(srcdir)/gdk-pixbuf-marshal.list
	case @GLIB_GENMARSHAL@ in									\
	   .*) glib_genmarshal=`pwd`/@GLIB_GENMARSHAL@ ;;				\
	   *) glib_genmarshal=@GLIB_GENMARSHAL@ ;;							\
	esac;												\
	cd $(srcdir) \
	&& $$glib_genmarshal --prefix=gdk_pixbuf_marshal gdk-pixbuf-marshal.list --header >> xgen-gmh	\
	&& (cmp -s xgen-gmh gdk-pixbuf-marshal.h || cp xgen-gmh gdk-pixbuf-marshal.h)			\
	&& rm -f xgen-gmh xgen-gmh~									\
	&& echo timestamp > $(@F)
stamp-gdk-pixbuf-marshal.c: @REBUILD@ $(srcdir)/gdk-pixbuf-marshal.list
	case @GLIB_GENMARSHAL@ in									\
	   .*) glib_genmarshal=`pwd`/@GLIB_GENMARSHAL@ ;;				\
	   *) glib_genmarshal=@GLIB_GENMARSHAL@ ;;							\
	esac;												\
	cd $(srcdir) \
	&& $$glib_genmarshal --prefix=gdk_pixbuf_marshal gdk-pixbuf-marshal.list --body >> xgen-gmc	\
	&& (cmp -s xgen-gmc gdk-pixbuf-marshal.c || cp xgen-gmc gdk-pixbuf-marshal.c)			\
	&& rm -f xgen-gmc xgen-gmc~									\
	&& echo timestamp > $(@F)

