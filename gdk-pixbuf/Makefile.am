include $(top_srcdir)/Makefile.decl

SUBDIRS = pixops

if PLATFORM_WIN32
no_undefined = -no-undefined
endif

if OS_WIN32
gdk_pixbuf_def = gdk_pixbuf.def
gdk_pixbuf_symbols = -export-symbols $(srcdir)/gdk_pixbuf.def

gdk_pixbuf_win32_res = gdk_pixbuf-win32-res.o
gdk_pixbuf_win32_res_ldflag = -Wl,gdk_pixbuf-win32-res.o

libole32 = -lole32

gdk_pixbuf-win32-res.o : gdk_pixbuf.rc
	$(WINDRES) gdk_pixbuf.rc $@

install-def-file:
	$(INSTALL) $(srcdir)/gdk_pixbuf.def $(DESTDIR)$(libdir)/gdk_pixbuf-$(GTK_API_VERSION).def
uninstall-def-file:
	-rm $(DESTDIR)$(libdir)/gdk_pixbuf-$(GTK_API_VERSION).def
else
install-def-file:
uninstall-def-file:
endif

if MS_LIB_AVAILABLE
noinst_DATA = gdk_pixbuf-$(GTK_API_VERSION).lib

gdk_pixbuf-$(GTK_API_VERSION).lib: libgdk_pixbuf-$(GTK_API_VERSION).la gdk_pixbuf.def
	lib -machine:@LIB_EXE_MACHINE_FLAG@ -name:libgdk_pixbuf-$(GTK_API_VERSION)-@LT_CURRENT_MINUS_AGE@.dll -def:gdk_pixbuf.def -out:$@

install-ms-lib:
	$(INSTALL) gdk_pixbuf-$(GTK_API_VERSION).lib $(DESTDIR)$(libdir)

uninstall-ms-lib:
	-rm $(DESTDIR)$(libdir)/gdk_pixbuf-$(GTK_API_VERSION).lib
else
install-ms-lib:
uninstall-ms-lib:
endif

# This places the generated .def file in srcdir, since it is expected to be there.
# (The one from a tarball is)
gdk_pixbuf.def: gdk-pixbuf.symbols
	(echo -e EXPORTS; $(CPP) -P -DINCLUDE_VARIABLES -DG_OS_WIN32 -DALL_FILES - <$(srcdir)/gdk-pixbuf.symbols | sed -e '/^$$/d' -e 's/^/	/' -e 's/G_GNUC_[^ ]*//g') > $(srcdir)/gdk_pixbuf.def

gdk-pixbuf-alias.h: gdk-pixbuf.symbols
	 $(PERL) $(srcdir)/makegdkpixbufalias.pl < $(srcdir)/gdk-pixbuf.symbols > gdk-pixbuf-alias.h

gdk-pixbuf-aliasdef.c: gdk-pixbuf.symbols
	 $(PERL) $(srcdir)/makegdkpixbufalias.pl -def < $(srcdir)/gdk-pixbuf.symbols > gdk-pixbuf-aliasdef.c

if OS_LINUX
TESTS = abicheck.sh pltcheck.sh
endif

lib_LTLIBRARIES = 		\
	libgdk_pixbuf-2.0.la

loaderdir = $(libdir)/gtk-2.0/$(GTK_BINARY_VERSION)/loaders

module_libs = libgdk_pixbuf-$(GTK_API_VERSION).la $(GDK_PIXBUF_DEP_LIBS)

#
# The PNG loader
#
libstatic_pixbufloader_png_la_SOURCES = io-png.c
libpixbufloader_png_la_SOURCES = io-png.c
libpixbufloader_png_la_LDFLAGS = -avoid-version -module $(no_undefined)
libpixbufloader_png_la_LIBADD = $(LIBPNG) $(module_libs)

#
# The BMP loader
#
libstatic_pixbufloader_bmp_la_SOURCES = io-bmp.c
libpixbufloader_bmp_la_SOURCES = io-bmp.c
libpixbufloader_bmp_la_LDFLAGS = -avoid-version -module $(no_undefined)
libpixbufloader_bmp_la_LIBADD = $(module_libs)

#
# The WBMP loader
#
libstatic_pixbufloader_wbmp_la_SOURCES = io-wbmp.c
libpixbufloader_wbmp_la_SOURCES = io-wbmp.c
libpixbufloader_wbmp_la_LDFLAGS = -avoid-version -module $(no_undefined)
libpixbufloader_wbmp_la_LIBADD = $(module_libs)

#
# The GIF loader
#
libstatic_pixbufloader_gif_la_SOURCES = io-gif.c io-gif-animation.c io-gif-animation.h
libpixbufloader_gif_la_SOURCES = io-gif.c io-gif-animation.c io-gif-animation.h
libpixbufloader_gif_la_LDFLAGS = -avoid-version -module $(no_undefined)
libpixbufloader_gif_la_LIBADD = $(module_libs)

#
# The ICO loader
#
libstatic_pixbufloader_ico_la_SOURCES = io-ico.c
libpixbufloader_ico_la_SOURCES = io-ico.c
libpixbufloader_ico_la_LDFLAGS = -avoid-version -module $(no_undefined)
libpixbufloader_ico_la_LIBADD = $(module_libs)

#
# The ANI loader
#
libstatic_pixbufloader_ani_la_SOURCES = io-ani.c io-ani-animation.c io-ani-animation.h
libpixbufloader_ani_la_SOURCES = io-ani.c io-ani-animation.c io-ani-animation.h
libpixbufloader_ani_la_LDFLAGS = -avoid-version -module $(no_undefined)
libpixbufloader_ani_la_LIBADD = $(module_libs)

#
# The JPEG loader
#
libstatic_pixbufloader_jpeg_la_SOURCES = io-jpeg.c
libpixbufloader_jpeg_la_SOURCES = io-jpeg.c
libpixbufloader_jpeg_la_LDFLAGS = -avoid-version -module $(no_undefined)
libpixbufloader_jpeg_la_LIBADD = $(LIBJPEG) $(module_libs)

#
# The PNM loader
#
libstatic_pixbufloader_pnm_la_SOURCES = io-pnm.c
libpixbufloader_pnm_la_SOURCES = io-pnm.c
libpixbufloader_pnm_la_LDFLAGS = -avoid-version -module $(no_undefined)
libpixbufloader_pnm_la_LIBADD = $(module_libs)

#
# The RAS loader
#
libstatic_pixbufloader_ras_la_SOURCES = io-ras.c
libpixbufloader_ras_la_SOURCES = io-ras.c
libpixbufloader_ras_la_LDFLAGS = -avoid-version -module $(no_undefined)
libpixbufloader_ras_la_LIBADD = $(module_libs)

#
# The TIFF loader
#
libstatic_pixbufloader_tiff_la_SOURCES = io-tiff.c
libpixbufloader_tiff_la_SOURCES = io-tiff.c
libpixbufloader_tiff_la_LDFLAGS = -avoid-version -module $(no_undefined)
libpixbufloader_tiff_la_LIBADD = $(LIBTIFF) $(module_libs)

#
# The XPM loader
#
libstatic_pixbufloader_xpm_la_SOURCES = io-xpm.c
libpixbufloader_xpm_la_SOURCES = io-xpm.c
libpixbufloader_xpm_la_LDFLAGS = -avoid-version -module $(no_undefined)
libpixbufloader_xpm_la_LIBADD = $(module_libs)

#
# The XBM loader
#
libstatic_pixbufloader_xbm_la_SOURCES = io-xbm.c
libpixbufloader_xbm_la_SOURCES = io-xbm.c
libpixbufloader_xbm_la_LDFLAGS = -avoid-version -module $(no_undefined)
libpixbufloader_xbm_la_LIBADD = $(module_libs)

#
# The TGA loader
#
libstatic_pixbufloader_tga_la_SOURCES = io-tga.c
libpixbufloader_tga_la_SOURCES = io-tga.c
libpixbufloader_tga_la_LDFLAGS = -avoid-version -module $(no_undefined)
libpixbufloader_tga_la_LIBADD = $(module_libs)

#
# The .icns loader
#
libstatic_pixbufloader_icns_la_SOURCES = io-icns.c
libpixbufloader_icns_la_SOURCES = io-icns.c
libpixbufloader_icns_la_LDFLAGS = -avoid-version -module $(no_undefined)
libpixbufloader_icns_la_LIBADD = $(module_libs)

#
# The PCX loader
#
libstatic_pixbufloader_pcx_la_SOURCES = io-pcx.c
libpixbufloader_pcx_la_SOURCES = io-pcx.c
libpixbufloader_pcx_la_LDFLAGS = -avoid-version -module $(no_undefined)
libpixbufloader_pcx_la_LIBADD = $(module_libs)

#
# The JPEG2000 loader
#
libstatic_pixbufloader_jasper_la_SOURCES = io-jasper.c
libpixbufloader_jasper_la_SOURCES = io-jasper.c
libpixbufloader_jasper_la_LDFLAGS = -avoid-version -module $(no_undefined)
libpixbufloader_jasper_la_LIBADD = $(LIBJASPER) $(module_libs)

#
# The QTIF loader
#
libstatic_pixbufloader_qtif_la_SOURCES = io-qtif.c
libpixbufloader_qtif_la_SOURCES = io-qtif.c
libpixbufloader_qtif_la_LDFLAGS = -avoid-version -module $(no_undefined)
libpixbufloader_qtif_la_LIBADD = $(module_libs)

if BUILD_GDIPLUS_LOADERS

if INCLUDE_GDIPLUS

# When building the GDI+ loader statically, we put the "common" objects
# only in one of the archives to avoid duplicate definitions

STATIC_GDIPLUS_LIBS = \
	libstatic-pixbufloader-gdip-ico.la \
	libstatic-pixbufloader-gdip-wmf.la \
	libstatic-pixbufloader-gdip-emf.la \
	libstatic-pixbufloader-gdip-bmp.la \
	libstatic-pixbufloader-gdip-gif.la \
	libstatic-pixbufloader-gdip-jpeg.la \
	libstatic-pixbufloader-gdip-tiff.la

libstatic_pixbufloader_gdip_ico_la_SOURCES = 	\
	io-gdip-native.h		\
	io-gdip-propertytags.h		\
	io-gdip-utils.h			\
	io-gdip-utils.c			\
	io-gdip-animation.c		\
	io-gdip-animation.h		\
	io-gdip-ico.c

libstatic_pixbufloader_gdip_wmf_la_SOURCES = 	\
	io-gdip-wmf.c

libstatic_pixbufloader_gdip_emf_la_SOURCES = 	\
	io-gdip-emf.c

libstatic_pixbufloader_gdip_bmp_la_SOURCES = 	\
	io-gdip-bmp.c

libstatic_pixbufloader_gdip_gif_la_SOURCES = 	\
	io-gdip-gif.c

libstatic_pixbufloader_gdip_jpeg_la_SOURCES = 	\
	io-gdip-jpeg.c

libstatic_pixbufloader_gdip_png_la_SOURCES = 	\
	io-gdip-png.c

libstatic_pixbufloader_gdip_tiff_la_SOURCES = 	\
	io-gdip-tiff.c

else

GDIPLUS_LIBS = \
	libpixbufloader-gdip-ico.la \
	libpixbufloader-gdip-wmf.la \
	libpixbufloader-gdip-emf.la \
	libpixbufloader-gdip-bmp.la \
	libpixbufloader-gdip-gif.la \
	libpixbufloader-gdip-jpeg.la \
	libpixbufloader-gdip-tiff.la

libpixbufloader_gdip_ico_la_LDFLAGS = -avoid-version -module -no-undefined
libpixbufloader_gdip_ico_la_SOURCES = 	\
	io-gdip-native.h		\
	io-gdip-propertytags.h		\
	io-gdip-utils.h			\
	io-gdip-utils.c			\
	io-gdip-animation.c		\
	io-gdip-animation.h		\
	io-gdip-ico.c
libpixbufloader_gdip_ico_la_LIBADD = $(module_libs) $(libole32)

libpixbufloader_gdip_wmf_la_LDFLAGS = -avoid-version -module -no-undefined
libpixbufloader_gdip_wmf_la_SOURCES = 	\
	io-gdip-native.h		\
	io-gdip-propertytags.h		\
	io-gdip-utils.h			\
	io-gdip-utils.c			\
	io-gdip-animation.c		\
	io-gdip-animation.h		\
	io-gdip-wmf.c
libpixbufloader_gdip_wmf_la_LIBADD = $(module_libs) $(libole32)

libpixbufloader_gdip_emf_la_LDFLAGS = -avoid-version -module -no-undefined
libpixbufloader_gdip_emf_la_SOURCES = 	\
	io-gdip-native.h		\
	io-gdip-propertytags.h		\
	io-gdip-utils.h			\
	io-gdip-utils.c			\
	io-gdip-animation.c		\
	io-gdip-animation.h		\
	io-gdip-emf.c
libpixbufloader_gdip_emf_la_LIBADD = $(module_libs) $(libole32)

libpixbufloader_gdip_bmp_la_LDFLAGS = -avoid-version -module -no-undefined
libpixbufloader_gdip_bmp_la_SOURCES = 	\
	io-gdip-native.h		\
	io-gdip-propertytags.h		\
	io-gdip-utils.h			\
	io-gdip-utils.c			\
	io-gdip-animation.c		\
	io-gdip-animation.h		\
	io-gdip-bmp.c
libpixbufloader_gdip_bmp_la_LIBADD = $(module_libs) $(libole32)

libpixbufloader_gdip_gif_la_LDFLAGS = -avoid-version -module -no-undefined
libpixbufloader_gdip_gif_la_SOURCES = 	\
	io-gdip-native.h		\
	io-gdip-propertytags.h		\
	io-gdip-utils.h			\
	io-gdip-utils.c			\
	io-gdip-animation.c		\
	io-gdip-animation.h		\
	io-gdip-gif.c
libpixbufloader_gdip_gif_la_LIBADD = $(module_libs) $(libole32)

libpixbufloader_gdip_jpeg_la_LDFLAGS = -avoid-version -module -no-undefined
libpixbufloader_gdip_jpeg_la_SOURCES = 	\
	io-gdip-native.h		\
	io-gdip-propertytags.h		\
	io-gdip-utils.h			\
	io-gdip-utils.c			\
	io-gdip-animation.c		\
	io-gdip-animation.h		\
	io-gdip-jpeg.c
libpixbufloader_gdip_jpeg_la_LIBADD = $(module_libs) $(libole32)

libpixbufloader_gdip_png_la_LDFLAGS = -avoid-version -module -no-undefined
libpixbufloader_gdip_png_la_SOURCES = 	\
	io-gdip-native.h		\
	io-gdip-propertytags.h		\
	io-gdip-utils.h			\
	io-gdip-utils.c			\
	io-gdip-animation.c		\
	io-gdip-animation.h		\
	io-gdip-png.c
libpixbufloader_gdip_png_la_LIBADD = $(module_libs) $(libole32)

libpixbufloader_gdip_tiff_la_LDFLAGS = -avoid-version -module -no-undefined
libpixbufloader_gdip_tiff_la_SOURCES = 	\
	io-gdip-native.h		\
	io-gdip-propertytags.h		\
	io-gdip-utils.h			\
	io-gdip-utils.c			\
	io-gdip-animation.c		\
	io-gdip-animation.h		\
	io-gdip-tiff.c
libpixbufloader_gdip_tiff_la_LIBADD = $(module_libs) $(libole32)

endif

else

# Loaders that aren't built if we build the GDI+ loader

if INCLUDE_BMP
STATIC_BMP_LIB = libstatic-pixbufloader-bmp.la
else
BMP_LIB = libpixbufloader-bmp.la
endif

if INCLUDE_GIF
STATIC_GIF_LIB = libstatic-pixbufloader-gif.la
else
GIF_LIB = libpixbufloader-gif.la
endif

if INCLUDE_ICO
STATIC_ICO_LIB = libstatic-pixbufloader-ico.la
else
ICO_LIB = libpixbufloader-ico.la
endif

if HAVE_JPEG
if INCLUDE_JPEG
STATIC_JPEG_LIB = libstatic-pixbufloader-jpeg.la
else
JPEG_LIB = libpixbufloader-jpeg.la
endif
endif

if HAVE_TIFF
if INCLUDE_TIFF
STATIC_TIFF_LIB = libstatic-pixbufloader-tiff.la
else
TIFF_LIB = libpixbufloader-tiff.la
endif
endif

# End of loaders not built if building GDI+ loader
endif

if HAVE_PNG
if INCLUDE_PNG
STATIC_PNG_LIB = libstatic-pixbufloader-png.la
else
PNG_LIB = libpixbufloader-png.la
endif
endif

if INCLUDE_WBMP
STATIC_WBMP_LIB = libstatic-pixbufloader-wbmp.la
else
WBMP_LIB = libpixbufloader-wbmp.la
endif

if INCLUDE_ANI
STATIC_ANI_LIB = libstatic-pixbufloader-ani.la
else
ANI_LIB = libpixbufloader-ani.la
endif

if INCLUDE_PNM
STATIC_PNM_LIB = libstatic-pixbufloader-pnm.la
else
PNM_LIB = libpixbufloader-pnm.la
endif

if INCLUDE_RAS
STATIC_RAS_LIB = libstatic-pixbufloader-ras.la
else
RAS_LIB = libpixbufloader-ras.la
endif

if INCLUDE_XPM
STATIC_XPM_LIB = libstatic-pixbufloader-xpm.la
else
XPM_LIB = libpixbufloader-xpm.la
endif

if INCLUDE_XBM
STATIC_XBM_LIB = libstatic-pixbufloader-xbm.la
else
XBM_LIB = libpixbufloader-xbm.la
endif

if INCLUDE_TGA
STATIC_TGA_LIB = libstatic-pixbufloader-tga.la
else
TGA_LIB = libpixbufloader-tga.la
endif

if INCLUDE_PCX
STATIC_PCX_LIB = libstatic-pixbufloader-pcx.la
else
PCX_LIB = libpixbufloader-pcx.la
endif

if INCLUDE_ICNS
STATIC_ICNS_LIB = libstatic-pixbufloader-icns.la
else
ICNS_LIB = libpixbufloader-icns.la
endif

if HAVE_JASPER
if INCLUDE_JASPER
STATIC_JASPER_LIB = libstatic-pixbufloader-jasper.la
else
JASPER_LIB = libpixbufloader-jasper.la
endif
endif

if INCLUDE_QTIF
STATIC_QTIF_LIB = libstatic-pixbufloader-qtif.la
else
QTIF_LIB = libpixbufloader-qtif.la
endif

if BUILD_DYNAMIC_MODULES

loader_LTLIBRARIES = 	\
	$(PNG_LIB)	\
	$(BMP_LIB)	\
	$(WBMP_LIB)	\
	$(GIF_LIB)	\
	$(ICO_LIB)	\
	$(ANI_LIB)	\
	$(JPEG_LIB)	\
	$(PNM_LIB)	\
	$(RAS_LIB)	\
	$(TIFF_LIB)	\
	$(XPM_LIB)	\
	$(XBM_LIB)	\
	$(TGA_LIB)	\
	$(ICNS_LIB)	\
	$(PCX_LIB)	\
	$(JASPER_LIB)	\
	$(QTIF_LIB)	\
	$(GDIPLUS_LIBS)


endif

noinst_LTLIBRARIES =		\
	$(STATIC_PNG_LIB)	\
	$(STATIC_BMP_LIB)	\
	$(STATIC_WBMP_LIB)	\
	$(STATIC_GIF_LIB)	\
	$(STATIC_ICO_LIB)	\
	$(STATIC_ANI_LIB)	\
	$(STATIC_JPEG_LIB)	\
	$(STATIC_PNM_LIB)	\
	$(STATIC_RAS_LIB)	\
	$(STATIC_TIFF_LIB)	\
	$(STATIC_XPM_LIB)	\
	$(STATIC_XBM_LIB)	\
	$(STATIC_TGA_LIB)	\
	$(STATIC_ICNS_LIB)	\
	$(STATIC_PCX_LIB)	\
	$(STATIC_JASPER_LIB)	\
	$(STATIC_QTIF_LIB)	\
	$(STATIC_GDIPLUS_LIBS)

builtin_objs = @INCLUDED_LOADER_OBJ@


DEPS = libgdk_pixbuf-$(GTK_API_VERSION).la
INCLUDES = \
	-DG_LOG_DOMAIN=\"GdkPixbuf\"		\
	-DGDK_PIXBUF_COMPILATION		\
	-I$(top_srcdir) -I$(top_builddir) 	\
	-I$(top_srcdir)/gdk-pixbuf 		\
	-I$(top_builddir)/gdk-pixbuf 		\
	-DGTK_SYSCONFDIR=\"$(sysconfdir)\"	\
	-DGTK_VERSION=\"$(GTK_VERSION)\"	\
	-DGTK_BINARY_VERSION=\"$(GTK_BINARY_VERSION)\"	\
	-DGTK_PREFIX=\"$(prefix)\"		\
	$(INCLUDED_LOADER_DEFINE) 		\
	$(GTK_DEBUG_FLAGS)			\
	$(GDK_PIXBUF_DEP_CFLAGS)		\
	-DGDK_PIXBUF_ENABLE_BACKEND

AM_CPPFLAGS = "-DPIXBUF_LIBDIR=\"$(loaderdir)\"" "-DBUILT_MODULES_DIR=\"$(srcdir)/.libs\""
LDADDS = libgdk_pixbuf-$(GTK_API_VERSION).la $(GDK_PIXBUF_DEP_LIBS)

noinst_PROGRAMS = test-gdk-pixbuf
test_gdk_pixbuf_LDADD = $(LDADDS)

bin_PROGRAMS = gdk-pixbuf-csource gdk-pixbuf-query-loaders
gdk_pixbuf_csource_SOURCES = gdk-pixbuf-csource.c
gdk_pixbuf_csource_LDADD = $(LDADDS)

gdk_pixbuf_query_loaders_DEPENDENCIES = $(DEPS)
gdk_pixbuf_query_loaders_LDADD = $(LDADDS)

gdk_pixbuf_query_loaders_SOURCES = queryloaders.c


#
# The GdkPixBuf library
#
libgdk_pixbufincludedir = $(includedir)/gtk-2.0/gdk-pixbuf
libgdk_pixbuf_2_0_la_SOURCES =   \
	gdk-pixbuf-i18n.h	 \
	gdk-pixbuf.c		 \
	gdk-pixbuf-animation.c	 \
	gdk-pixbuf-data.c	 \
	gdk-pixbuf-io.c		 \
	gdk-pixbuf-loader.c	 \
	gdk-pixbuf-scale.c	 \
	gdk-pixbuf-simple-anim.c \
	gdk-pixbuf-scaled-anim.c \
	gdk-pixbuf-util.c	 \
	gdk-pixdata.c		 \
	gdk-pixbuf-enum-types.c

libgdk_pixbuf_2_0_la_LDFLAGS = \
	$(gdk_pixbuf_win32_res_ldflag)		\
	-version-info $(LT_VERSION_INFO)	\
	$(LIBTOOL_EXPORT_OPTIONS)		\
	$(no_undefined)				\
	$(gdk_pixbuf_symbols)


libgdk_pixbuf_2_0_la_LIBADD = pixops/libpixops.la $(builtin_objs) $(GDK_PIXBUF_DEP_LIBS) $(libole32)
libgdk_pixbuf_2_0_la_DEPENDENCIES = pixops/libpixops.la $(builtin_objs) $(gdk_pixbuf_def) $(gdk_pixbuf_win32_res)

gdk_pixbuf_headers = 			\
	gdk-pixbuf.h			\
	gdk-pixbuf-core.h		\
	gdk-pixbuf-transform.h		\
	gdk-pixbuf-io.h			\
	gdk-pixbuf-animation.h		\
	gdk-pixbuf-simple-anim.h	\
	gdk-pixbuf-loader.h

libgdk_pixbufinclude_HEADERS =  	\
	$(gdk_pixbuf_headers)		\
	gdk-pixbuf-enum-types.h		\
	gdk-pixbuf-marshal.h		\
	gdk-pixbuf-features.h		\
	gdk-pixdata.h

noinst_HEADERS = 		\
	gdk-pixbuf-alias.h	\
	gdk-pixbuf-private.h	\
	gdk-pixbuf-scaled-anim.h \
	xpm-color-table.h

BUILT_SOURCES = 		\
	gdk-pixbuf-alias.h	\
	gdk-pixbuf-aliasdef.c	\
	gdk-pixbuf-enum-types.h \
	gdk-pixbuf-enum-types.c \
	gdk-pixbuf-marshal.h 	\
	gdk-pixbuf-marshal.c	\
	gdk-pixbuf-features.h

CLEANFILES =
MAINTAINERCLEANFILES = 		\
	gdk-pixbuf-aliasdef.c	\
	gdk-pixbuf-enum-types.h \
	gdk-pixbuf-enum-types.c \
	gdk-pixbuf-marshal.h 	\
	gdk-pixbuf-marshal.c 	\
	gdk-pixbuf.loaders

#
# gdk-pixbuf-enum-types.h
#
gdk-pixbuf-enum-types.h: s-enum-types-h
	@true

s-enum-types-h: @REBUILD@ $(gdk_pixbuf_headers) gdk-pixbuf-enum-types.h.template
	( cd $(srcdir) && $(GLIB_MKENUMS) --template gdk-pixbuf-enum-types.h.template \
		$(gdk_pixbuf_headers) ) > tmp-gdk-pixbuf-enum-types.h \
	&& (cmp -s tmp-gdk-pixbuf-enum-types.h gdk-pixbuf-enum-types.h || cp tmp-gdk-pixbuf-enum-types.h gdk-pixbuf-enum-types.h ) \
	&& rm -f tmp-gdk-pixbuf-enum-types.h \
	&& echo timestamp > $(@F)

CLEANFILES += tmp-gdk-pixbuf-enum-types.h
MAINTAINERCLEANFILES += s-enum-types-h

#
# gdk-pixbuf-enum-types.c
#
gdk-pixbuf-enum-types.c: @REBUILD@ $(gdk_pixbuf_headers) gdk-pixbuf-enum-types.c.template
	(cd $(srcdir) && $(GLIB_MKENUMS) --template gdk-pixbuf-enum-types.c.template \
		  $(gdk_pixbuf_headers)) > gdk-pixbuf-enum-types.c

#
# gdk-pixbuf-marshal.h
#
gdk-pixbuf-marshal.h: @REBUILD@ stamp-gdk-pixbuf-marshal.h
	@true

stamp-gdk-pixbuf-marshal.h: @REBUILD@ $(srcdir)/gdk-pixbuf-marshal.list
	echo "#if !defined(GDK_PIXBUF_DISABLE_DEPRECATED) || defined(GDK_PIXBUF_COMPILATION)" > xgen-gmh \
	&& $(GLIB_GENMARSHAL) --prefix=_gdk_pixbuf_marshal $(srcdir)/gdk-pixbuf-marshal.list --header >> xgen-gmh \
	&& echo "#endif /* !GDK_PIXBUF_DISABLE_DEPRECATED || GDK_PIXBUF_COMPILATION */" >> xgen-gmh \
	&& (cmp -s xgen-gmh gdk-pixbuf-marshal.h || cp xgen-gmh gdk-pixbuf-marshal.h) \
	&& rm -f xgen-gmh xgen-gmh~ \
	&& echo timestamp > $(@F)

CLEANFILES += xgen-gmh
MAINTAINERCLEANFILES += stamp-gdk-pixbuf-marshal.h

#
# gdk-pixbuf-marshal.c
#
$(srcdir)/gdk-pixbuf-marshal.c: @REBUILD@ $(srcdir)/gdk-pixbuf-marshal.list
	(echo -e "#include <gdk-pixbuf/gdk-pixbuf.h>\n#include \"gdk-pixbuf-alias.h\"" | $(GLIB_GENMARSHAL) --prefix=_gdk_pixbuf_marshal $(srcdir)/gdk-pixbuf-marshal.list --body ) >> xgen-gmc \
	&& cp xgen-gmc gdk-pixbuf-marshal.c \
	&& rm -f xgen-gmc xgen-gmc~

CLEANFILES += xgen-gmc

# if srcdir!=builddir, clean out maintainer-clean files from builddir
# this allows dist to pass.
distclean-local:
	if test $(srcdir) != .; then \
	  rm -f $(MAINTAINERCLEANFILES); \
	fi

EXTRA_DIST +=					\
        gdk-pixbuf-csource.1			\
	makefile.msc				\
	gdk-pixbuf.symbols			\
	makegdkpixbufalias.pl			\
	abicheck.sh				\
	pltcheck.sh				\
	gdk_pixbuf.def				\
	gdk_pixbuf.rc				\
	gdk-pixbuf-marshal.c			\
	gdk-pixbuf-marshal.list			\
	gdk-pixbuf-enum-types.c.template	\
	gdk-pixbuf-enum-types.h.template	\
	gen-color-table.pl

if CROSS_COMPILING
RUN_QUERY_LOADER_TEST=false
else
RUN_QUERY_LOADER_TEST=test -z "$(DESTDIR)"
endif

# Running this if cross compiling or if DESTDIR is set is going to
# not work at all, so skip it
# We use install-data-hook here to workaround a bug in automake and/or libtool
# that makes the install target for the loader libraries a dependency on
# install-data-am, and not install-exec-am. We need to ensure this gets run
# after the libraries are installed in their final locations.
install-data-hook: install-ms-lib install-def-file
	@if $(RUN_QUERY_LOADER_TEST) ; then \
	  $(mkinstalldirs) $(DESTDIR)$(sysconfdir)/gtk-2.0 ; \
	  $(top_builddir)/gdk-pixbuf/gdk-pixbuf-query-loaders > $(DESTDIR)$(sysconfdir)/gtk-2.0/gdk-pixbuf.loaders ; \
	else \
	  echo "***" ; \
	  echo "*** Warning: gdk-pixbuf.loaders not built" ; \
	  echo "***" ; \
	  echo "*** Generate this file manually on host" ; \
	  echo "*** system using gdk-pixbuf-query-loaders" ; \
	  echo "***" ; \
	fi

uninstall-local: uninstall-ms-lib uninstall-def-file
	rm -f $(DESTDIR)$(sysconfdir)/gtk-2.0/gdk-pixbuf.loaders

if CROSS_COMPILING
else
all-local: gdk-pixbuf.loaders
endif

if BUILD_DYNAMIC_MODULES
gdk-pixbuf.loaders: $(loader_LTLIBRARIES) gdk-pixbuf-query-loaders$(EXEEXT)
	LOADERS=`echo libpixbufloader-*.la` ; \
	if test "x$$LOADERS" != 'xlibpixbufloader-*.la' ; then \
          echo "Writing a gdk-pixbuf.loader file to use when running examples before installing gdk-pixbuf."; \
	  $(top_builddir)/gdk-pixbuf/gdk-pixbuf-query-loaders $$LOADERS > ./gdk-pixbuf.loaders ;\
	else \
          echo "No dynamic modules found; will use only static modules for uninstalled example programs."; \
	  touch gdk-pixbuf.loaders; \
	fi
else
gdk-pixbuf.loaders: 
	echo "No dynamic modules found; will use only static modules for uninstalled example programs."; \
	touch gdk-pixbuf.loaders;
endif

-include $(top_srcdir)/git.mk
