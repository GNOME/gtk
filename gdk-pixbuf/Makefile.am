
SUBDIRS = pixops

if PLATFORM_WIN32
no_undefined = -no-undefined
endif

if OS_WIN32
gdk_pixbuf_def = gdk_pixbuf.def
gdk_pixbuf_symbols = -export-symbols $(gdk_pixbuf_def)

install-libtool-import-lib:
	$(INSTALL) .libs/libgdk_pixbuf-1.3.dll.a $(DESTDIR)$(libdir)
uninstall-libtool-import-lib:
	-rm $(DESTDIR)$(libdir)/libdk_pixbuf-1.3.dll.a
else
install-libtool-import-lib:
uninstall-libtool-import-lib:
endif

if MS_LIB_AVAILABLE
noinst_DATA = gdk_pixbuf-1.3.lib

gdk_pixbuf-1.3.lib: libgdk_pixbuf-1.3.la gdk_pixbuf.def
	lib -name:libgdk_pixbuf-1.3-@LT_CURRENT_MINUS_AGE@.dll -def:gdk_pixbuf.def -out:$@

install-ms-lib:
	$(INSTALL) gdk_pixbuf-1.3.lib $(DESTDIR)$(libdir)

uninstall-ms-lib:
	-rm $(DESTDIR)$(libdir)/gdk_pixbuf-1.3.lib
else
install-ms-lib:
uninstall-ms-lib:
endif

lib_LTLIBRARIES = 		\
	libgdk_pixbuf-1.3.la

loaderdir = $(libdir)/gtk-2.0/$(GTK_VERSION)/loaders

module_libs = libgdk_pixbuf-1.3.la $(GDK_PIXBUF_DEP_LIBS)

#
# The PNG plugin.
#
libpixbufloader_png_la_SOURCES = io-png.c
libpixbufloader_png_la_LDFLAGS = -avoid-version -module $(no_undefined)
libpixbufloader_png_la_LIBADD = $(LIBPNG) $(module_libs)
libpixbufloader_static_png_la_SOURCES = io-png.c

#
# The JPEG loader
#
libpixbufloader_static_jpeg_la_SOURCES = io-jpeg.c
libpixbufloader_jpeg_la_SOURCES = io-jpeg.c
libpixbufloader_jpeg_la_LDFLAGS = -avoid-version -module $(no_undefined)
libpixbufloader_jpeg_la_LIBADD = $(LIBJPEG) $(module_libs)

#
# The XPM loader
#
libpixbufloader_static_xpm_la_SOURCES = io-xpm.c
libpixbufloader_xpm_la_SOURCES = io-xpm.c
libpixbufloader_xpm_la_LDFLAGS = -avoid-version -module $(no_undefined)
libpixbufloader_xpm_la_LIBADD = $(module_libs)

#
# The GIF loader
#
libpixbufloader_static_gif_la_SOURCES = io-gif.c io-gif-animation.c io-gif-animation.h
libpixbufloader_gif_la_SOURCES = io-gif.c io-gif-animation.c io-gif-animation.h
libpixbufloader_gif_la_LDFLAGS = -avoid-version -module $(no_undefined)
libpixbufloader_gif_la_LIBADD = $(module_libs)

#
# The ICO loader
#
libpixbufloader_static_ico_la_SOURCES = io-ico.c
libpixbufloader_ico_la_SOURCES = io-ico.c
libpixbufloader_ico_la_LDFLAGS = -avoid-version -module $(no_undefined)
libpixbufloader_ico_la_LIBADD = $(module_libs)

#
# The RAS loader
#
libpixbufloader_static_ras_la_SOURCES = io-ras.c
libpixbufloader_ras_la_SOURCES = io-ras.c
libpixbufloader_ras_la_LDFLAGS = -avoid-version -module $(no_undefined)
libpixbufloader_ras_la_LIBADD = $(module_libs)

#
# The TIFF loader
#
libpixbufloader_static_tiff_la_SOURCES = io-tiff.c
libpixbufloader_tiff_la_SOURCES = io-tiff.c
libpixbufloader_tiff_la_LDFLAGS = -avoid-version -module $(no_undefined)
libpixbufloader_tiff_la_LIBADD = $(LIBTIFF) $(module_libs)

#
# The PNM loader
#
libpixbufloader_static_pnm_la_SOURCES = io-pnm.c
libpixbufloader_pnm_la_SOURCES = io-pnm.c
libpixbufloader_pnm_la_LDFLAGS = -avoid-version -module $(no_undefined)
libpixbufloader_pnm_la_LIBADD = $(module_libs)

#
# The BMP loader
#
libpixbufloader_static_bmp_la_SOURCES = io-bmp.c
libpixbufloader_bmp_la_SOURCES = io-bmp.c
libpixbufloader_bmp_la_LDFLAGS = -avoid-version -module $(no_undefined)
libpixbufloader_bmp_la_LIBADD = $(module_libs)

#
# The WBMP loader
#
libpixbufloader_static_wbmp_la_SOURCES = io-wbmp.c
libpixbufloader_wbmp_la_SOURCES = io-wbmp.c
libpixbufloader_wbmp_la_LDFLAGS = -avoid-version -module $(no_undefined)
libpixbufloader_wbmp_la_LIBADD = $(module_libs)

#
# The XBM loader
#
libpixbufloader_static_xbm_la_SOURCES = io-xbm.c
libpixbufloader_xbm_la_SOURCES = io-xbm.c
libpixbufloader_xbm_la_LDFLAGS = -avoid-version -module $(no_undefined)
libpixbufloader_xbm_la_LIBADD = $(module_libs)

#
# The TGA loader
#
libpixbufloader_static_tga_la_SOURCES = io-tga.c
libpixbufloader_tga_la_SOURCES = io-tga.c
libpixbufloader_tga_la_LDFLAGS = -avoid-version -module $(no_undefined)
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
	libpixbufloader-static-wbmp.la \
	libpixbufloader-static-xbm.la  \
	libpixbufloader-static-tga.la

builtin_objs = @INCLUDED_LOADER_OBJ@

endif

DEPS = libgdk_pixbuf-1.3.la
INCLUDES = @STRIP_BEGIN@ 			\
	-DG_LOG_DOMAIN=\"GdkPixbuf\"		\
	-I$(top_srcdir) -I$(top_builddir) 	\
	-I$(top_srcdir)/gdk-pixbuf 		\
	-I$(top_builddir)/gdk-pixbuf 		\
	-DGTK_VERSION=\"@GTK_VERSION@\"		\
	-DG_DISABLE_DEPRECATED			\
	-DGDK_PIXBUF_DISABLE_DEPRECATED		\
	@INCLUDED_LOADER_DEFINE@ 		\
	@GTK_DEBUG_FLAGS@			\
	@GDK_PIXBUF_DEP_CFLAGS@			\
@STRIP_END@

AM_CPPFLAGS = "-DPIXBUF_LIBDIR=\"$(loaderdir)\"" "-DBUILT_MODULES_DIR=\"$(srcdir)/.libs\""
LDADDS = libgdk_pixbuf-1.3.la

noinst_PROGRAMS = test-gdk-pixbuf test-loaders
test_gdk_pixbuf_LDADD = $(LDADDS)
test_loaders_LDADD = $(LDADDS)
test_loaders_SOURCES = test-loaders.c test-images.h

bin_PROGRAMS = gdk-pixbuf-csource
gdk_pixbuf_csource_SOURCES = gdk-pixbuf-csource.c
gdk_pixbuf_csource_LDADD = $(LDADDS)

#
# manual pages to install
#
man_MANS = gdk-pixbuf-csource.1


#
# The GdkPixBuf library
#
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
	$(gdk_pixbuf_built_cfiles) \
@STRIP_END@
libgdk_pixbuf_1_3_la_LDFLAGS = @STRIP_BEGIN@ \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)    \
	@LIBTOOL_EXPORT_OPTIONS@				\
	$(no_undefined)						\
	$(gdk_pixbuf_symbols)					\
@STRIP_END@

libgdk_pixbuf_1_3_la_LIBADD = pixops/libpixops.la $(builtin_objs) $(GDK_PIXBUF_DEP_LIBS)
libgdk_pixbuf_1_3_la_DEPENDENCIES = pixops/libpixops.la $(builtin_objs) $(gdk_pixbuf_def)

gdk_pixbuf_headers = 			\
	gdk-pixbuf.h			\
	gdk-pixbuf-loader.h

libgdk_pixbufinclude_HEADERS =  	\
	$(gdk_pixbuf_headers)		\
	$(gdk_pixbuf_built_headers)	\
	gdk-pixbuf-features.h		\
	gdk-pixdata.h

noinst_HEADERS = 		\
	gdk-pixbuf-io.h		\
	gdk-pixbuf-private.h

gdk_pixbuf_built_headers = gdk-pixbuf-enum-types.h gdk-pixbuf-marshal.h
gdk_pixbuf_built_cfiles = gdk-pixbuf-enum-types.c 
$(libgdk_pixbuf_1_3_la_OBJECTS) $(OBJECTS): $(gdk_pixbuf_built_headers) $(gdk_pixbuf_built_cfiles) gdk-pixbuf-marshal.c

CLEANFILES =
MAINTAINERCLEANFILES = $(gdk_pixbuf_built_headers) $(gdk_pixbuf_built_cfiles) gdk-pixbuf-marshal.c

#
# gdk-pixbuf-enum-types.h
#
$(srcdir)/gdk-pixbuf-enum-types.h: s-enum-types-h
	@true

s-enum-types-h: @REBUILD@ $(gdk_pixbuf_headers) Makefile
	( cd $(srcdir) && glib-mkenums \
			--fhead "#ifndef __GDK_PIXBUF_ENUM_TYPES_H__\n#define __GDK_PIXBUF_ENUM_TYPES_H__\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n" \
			--fprod "/* enumerations from \"@filename@\" */\n" \
			--vhead "GType @enum_name@_get_type (void);\n#define GDK_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n"         \
			--ftail "G_END_DECLS\n\n#endif /* __GDK_PIXBUF_ENUM_TYPES_H__ */" \
		$(gdk_pixbuf_headers) ) > tmp-gdk-pixbuf-enum-types.h \
	&& (cmp -s tmp-gdk-pixbuf-enum-types.h $(srcdir)/gdk-pixbuf-enum-types.h || cp tmp-gdk-pixbuf-enum-types.h $(srcdir)/gdk-pixbuf-enum-types.h ) \
	&& rm -f tmp-gdk-pixbuf-enum-types.h \
	&& echo timestamp > $(@F)

CLEANFILES += tmp-gdk-pixbuf-enum-types.h
MAINTAINERCLEANFILES += s-enum-types-h

#
# gdk-pixbuf-enum-types.c
#
$(srcdir)/gdk-pixbuf-enum-types.c: @REBUILD@ $(gdk_pixbuf_headers) Makefile
	cd $(srcdir) && glib-mkenums \
		--fhead "#include <gdk-pixbuf/gdk-pixbuf.h>" \
		--fprod "\n/* enumerations from \"@filename@\" */" \
		--vhead "GType\n@enum_name@_get_type (void)\n{\n  static GType etype = 0;\n  if (etype == 0) {\n    static const G@Type@Value values[] = {"       \
		--vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
		--vtail "      { 0, NULL, NULL }\n    };\n    etype = g_@type@_register_static (\"@EnumName@\", values);\n  }\n  return etype;\n}\n" \
		  $(gdk_pixbuf_headers) > gdk-pixbuf-enum-types.c

#
# gdk-pixbuf-marshal.h
#
$(srcdir)/gdk-pixbuf-marshal.h: @REBUILD@ stamp-gdk-pixbuf-marshal.h
	@true

stamp-gdk-pixbuf-marshal.h: @REBUILD@ $(srcdir)/gdk-pixbuf-marshal.list
	case @GLIB_GENMARSHAL@ in									\
	   .*) glib_genmarshal=`pwd`/@GLIB_GENMARSHAL@ ;;				\
	   *) glib_genmarshal=@GLIB_GENMARSHAL@ ;;							\
	esac;												\
	cd $(srcdir) \
	&& $$glib_genmarshal --prefix=gdk_pixbuf_marshal gdk-pixbuf-marshal.list --header >> xgen-gmh	\
	&& (cmp -s xgen-gmh gdk-pixbuf-marshal.h || cp xgen-gmh gdk-pixbuf-marshal.h)			\
	&& rm -f xgen-gmh xgen-gmh~	\
	&& echo timestamp > $(@F)

CLEANFILES += xgen-gmh
MAINTAINERCLEANFILES += stamp-gdk-pixbuf-marshal.h

#
# gdk-pixbuf-marshal.c
#
$(srcdir)/gdk-pixbuf-marshal.c: @REBUILD@ $(srcdir)/gdk-pixbuf-marshal.list
	case @GLIB_GENMARSHAL@ in									\
	   .*) glib_genmarshal=`pwd`/@GLIB_GENMARSHAL@ ;;				\
	   *) glib_genmarshal=@GLIB_GENMARSHAL@ ;;							\
	esac;												\
	cd $(srcdir) \
	&& $$glib_genmarshal --prefix=gdk_pixbuf_marshal gdk-pixbuf-marshal.list --body >> xgen-gmc	\
	&& cp xgen-gmc gdk-pixbuf-marshal.c								\
	&& rm -f xgen-gmc xgen-gmc~

CLEANFILES += xgen-gmc

EXTRA_DIST =			\
        gdk-pixbuf-csource.1    \
	makefile.mingw		\
	makefile.mingw.in	\
	makefile.msc		\
	gdk_pixbuf.def		\
	gdk-pixbuf-marshal.c	\
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
	pixbufloader_xbm.def		\
	pixbufloader_tga.def

install-data-local: install-ms-lib install-libtool-import-lib

uninstall-local: uninstall-ms-lib uninstall-libtool-import-lib
