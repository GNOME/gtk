
SUBDIRS = pixops

if PLATFORM_WIN32
no_undefined = -no-undefined
endif

if OS_WIN32
gdk_pixbuf_def = gdk_pixbuf.def
gdk_pixbuf_symbols = -export-symbols gdk_pixbuf.def

gdk_pixbuf_win32_res = gdk_pixbuf-win32-res.o
gdk_pixbuf_win32_res_ldflag = -Wl,gdk_pixbuf-win32-res.o

gdk_pixbuf-win32-res.o : gdk_pixbuf.rc
	$(WINDRES) gdk_pixbuf.rc $@

install-def-file:
	$(INSTALL) gdk_pixbuf.def $(DESTDIR)$(libdir)/gdk_pixbuf-$(GTK_API_VERSION).def
uninstall-def-file:
	-rm $(DESTDIR)$(libdir)/gdk_pixbuf-$(GTK_API_VERSION).def
else
install-def-file:
uninstall-def-file:
endif

if MS_LIB_AVAILABLE
noinst_DATA = gdk_pixbuf-$(GTK_API_VERSION).lib

gdk_pixbuf-$(GTK_API_VERSION).lib: libgdk_pixbuf-$(GTK_API_VERSION).la gdk_pixbuf.def
	lib -name:libgdk_pixbuf-$(GTK_API_VERSION)-@LT_CURRENT_MINUS_AGE@.dll -def:gdk_pixbuf.def -out:$@

install-ms-lib:
	$(INSTALL) gdk_pixbuf-$(GTK_API_VERSION).lib $(DESTDIR)$(libdir)

uninstall-ms-lib:
	-rm $(DESTDIR)$(libdir)/gdk_pixbuf-$(GTK_API_VERSION).lib
else
install-ms-lib:
uninstall-ms-lib:
endif

gdk_pixbuf.def: gdk-pixbuf.symbols
	(echo -e EXPORTS; $(CPP) -P -DINCLUDE_VARIABLES -DG_OS_WIN32 -DALL_FILES - <$(srcdir)/gdk-pixbuf.symbols | sed -e '/^$$/d' -e 's/^/	/' -e 's/G_GNUC_[^ ]*//g') > gdk_pixbuf.def

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
# The ANI loader
#
libpixbufloader_static_ani_la_SOURCES = io-ani.c io-ani-animation.c io-ani-animation.h
libpixbufloader_ani_la_SOURCES = io-ani.c io-ani-animation.c io-ani-animation.h
libpixbufloader_ani_la_LDFLAGS = -avoid-version -module $(no_undefined)
libpixbufloader_ani_la_LIBADD = $(module_libs)

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

#
# The PCX loader
#
libpixbufloader_static_pcx_la_SOURCES = io-pcx.c
libpixbufloader_pcx_la_SOURCES = io-pcx.c
libpixbufloader_pcx_la_LDFLAGS = -avoid-version -module $(no_undefined)
libpixbufloader_pcx_la_LIBADD = $(module_libs)

if HAVE_PNG
PNG_LIB = libpixbufloader-png.la
STATIC_PNG_LIB = libpixbufloader-static-png.la
endif

if HAVE_JPEG
JPEG_LIB = libpixbufloader-jpeg.la
STATIC_JPEG_LIB = libpixbufloader-static-jpeg.la
endif

GIF_LIB = libpixbufloader-gif.la
STATIC_GIF_LIB = libpixbufloader-static-gif.la

ICO_LIB = libpixbufloader-ico.la
STATIC_ICO_LIB = libpixbufloader-static-ico.la

ANI_LIB = libpixbufloader-ani.la
STATIC_ANI_LIB = libpixbufloader-static-ani.la

RAS_LIB = libpixbufloader-ras.la
STATIC_RAS_LIB = libpixbufloader-static-ras.la

if HAVE_TIFF
TIFF_LIB = libpixbufloader-tiff.la
STATIC_TIFF_LIB = libpixbufloader-static-tiff.la
endif

XPM_LIB = libpixbufloader-xpm.la
STATIC_XPM_LIB = libpixbufloader-static-xpm.la

PNM_LIB = libpixbufloader-pnm.la
STATIC_PNM_LIB = libpixbufloader-static-pnm.la

BMP_LIB = libpixbufloader-bmp.la
STATIC_BMP_LIB = libpixbufloader-static-bmp.la

WBMP_LIB = libpixbufloader-wbmp.la
STATIC_WBMP_LIB = libpixbufloader-static-wbmp.la

XBM_LIB = libpixbufloader-xbm.la
STATIC_XBM_LIB = libpixbufloader-static-xbm.la

TGA_LIB = libpixbufloader-tga.la
STATIC_TGA_LIB = libpixbufloader-static-tga.la

PCX_LIB = libpixbufloader-pcx.la
STATIC_PCX_LIB = libpixbufloader-static-pcx.la

if BUILD_DYNAMIC_MODULES

loader_LTLIBRARIES = 	\
	$(PNG_LIB)	\
	$(JPEG_LIB)	\
	$(GIF_LIB)	\
	$(ICO_LIB)	\
	$(ANI_LIB)	\
	$(RAS_LIB)	\
	$(XPM_LIB)	\
	$(TIFF_LIB)	\
	$(PNM_LIB)	\
	$(BMP_LIB)	\
	$(WBMP_LIB)	\
	$(XBM_LIB)	\
	$(TGA_LIB)	\
	$(PCX_LIB)


extra_sources = 
builtin_objs = 
else
loader_LTLIBRARIES = 

noinst_LTLIBRARIES =		\
	$(STATIC_PNG_LIB)	\
	$(STATIC_JPEG_LIB)	\
	$(STATIC_GIF_LIB)	\
	$(STATIC_ICO_LIB)	\
	$(STATIC_ANI_LIB)	\
	$(STATIC_RAS_LIB)	\
	$(STATIC_XPM_LIB)	\
	$(STATIC_TIFF_LIB)	\
	$(STATIC_PNM_LIB)	\
	$(STATIC_BMP_LIB)	\
	$(STATIC_WBMP_LIB)	\
	$(STATIC_XBM_LIB)	\
	$(STATIC_TGA_LIB)	\
	$(STATIC_PCX_LIB)

builtin_objs = @INCLUDED_LOADER_OBJ@

endif

DEPS = libgdk_pixbuf-$(GTK_API_VERSION).la
INCLUDES = \
	-DG_LOG_DOMAIN=\"GdkPixbuf\"		\
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
LDADDS = libgdk_pixbuf-$(GTK_API_VERSION).la $(GLIB_LIBS)

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
	gdk-pixbuf-util.c	 \
	gdk-pixdata.c		 \
	gdk-pixbuf-enum-types.c

libgdk_pixbuf_2_0_la_LDFLAGS = \
	$(gdk_pixbuf_win32_res_ldflag)		\
	-version-info $(LT_VERSION_INFO)	\
	$(LIBTOOL_EXPORT_OPTIONS)		\
	$(no_undefined)				\
	$(gdk_pixbuf_symbols)


libgdk_pixbuf_2_0_la_LIBADD = pixops/libpixops.la $(builtin_objs) $(GDK_PIXBUF_DEP_LIBS)
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
	xpm-color-table.h

BUILT_SOURCES = 		\
	gdk-pixbuf-alias.h	\
	gdk-pixbuf-aliasdef.c	\
	gdk-pixbuf-enum-types.h \
	gdk-pixbuf-enum-types.c \
	gdk-pixbuf-marshal.h 	\
	gdk-pixbuf-marshal.c

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

s-enum-types-h: @REBUILD@ $(gdk_pixbuf_headers) Makefile
	( cd $(srcdir) && $(GLIB_MKENUMS) \
			--fhead "#ifndef __GDK_PIXBUF_ENUM_TYPES_H__\n#define __GDK_PIXBUF_ENUM_TYPES_H__\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n" \
			--fprod "/* enumerations from \"@filename@\" */\n" \
			--vhead "GType @enum_name@_get_type (void) G_GNUC_CONST;\n#define GDK_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n"         \
			--ftail "G_END_DECLS\n\n#endif /* __GDK_PIXBUF_ENUM_TYPES_H__ */" \
		$(gdk_pixbuf_headers) ) > tmp-gdk-pixbuf-enum-types.h \
	&& (cmp -s tmp-gdk-pixbuf-enum-types.h gdk-pixbuf-enum-types.h || cp tmp-gdk-pixbuf-enum-types.h gdk-pixbuf-enum-types.h ) \
	&& rm -f tmp-gdk-pixbuf-enum-types.h \
	&& echo timestamp > $(@F)

CLEANFILES += tmp-gdk-pixbuf-enum-types.h
MAINTAINERCLEANFILES += s-enum-types-h

#
# gdk-pixbuf-enum-types.c
#
gdk-pixbuf-enum-types.c: @REBUILD@ $(gdk_pixbuf_headers) Makefile
	(cd $(srcdir) && $(GLIB_MKENUMS) \
		--fhead "#include <gdk-pixbuf/gdk-pixbuf.h>\n#include \"gdk-pixbuf-alias.h\"\n" \
		--fprod "\n/* enumerations from \"@filename@\" */" \
		--ftail "\n#define __GDK_PIXBUF_ENUM_TYPES_C__\n#include \"gdk-pixbuf-aliasdef.c\"" \
		--vhead "GType\n@enum_name@_get_type (void)\n{\n  static GType etype = 0;\n  if (etype == 0) {\n    static const G@Type@Value values[] = {"       \
		--vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
		--vtail "      { 0, NULL, NULL }\n    };\n    etype = g_@type@_register_static (g_intern_static_string (\"@EnumName@\"), values);\n  }\n  return etype;\n}\n" \
		  $(gdk_pixbuf_headers)) > gdk-pixbuf-enum-types.c

#
# gdk-pixbuf-marshal.h
#
gdk-pixbuf-marshal.h: @REBUILD@ stamp-gdk-pixbuf-marshal.h
	@true

stamp-gdk-pixbuf-marshal.h: @REBUILD@ $(srcdir)/gdk-pixbuf-marshal.list
	$(GLIB_GENMARSHAL) --prefix=_gdk_pixbuf_marshal $(srcdir)/gdk-pixbuf-marshal.list --header >> xgen-gmh \
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

EXTRA_DIST =					\
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
