## Makefile.am for gtk+/gdk

SUBDIRS = $(gdktarget)
DIST_SUBDIRS = linux-fb win32 x11

EXTRA_DIST =			\
	gdkconfig.h.win32 	\
	gdk.def 		\
	gdkmarshalers.list	\
	makeenums.pl		\
	makefile.msc		\
	gdk.symbols		\
	makegdkalias.pl		\
	abicheck.sh

INCLUDES =                              \
	-DG_LOG_DOMAIN=\"Gdk\"		\
	-DGDK_COMPILATION		\
	-I$(top_srcdir)			\
	-I$(top_builddir)/gdk		\
	-I$(top_srcdir)/gdk-pixbuf 	\
	-DG_DISABLE_DEPRECATED		\
	-DGDK_PIXBUF_DISABLE_DEPRECATED	\
	-DGDK_DISABLE_DEPRECATED	\
	$(GTK_DEBUG_FLAGS) 		\
	$(GDK_DEP_CFLAGS)

gtarget=$(gdktarget)

if PLATFORM_WIN32
no_undefined = -no-undefined
endif

# libtool stuff: set version and export symbols for resolving
# since automake doesn't support conditionalized libsomething_la_LDFLAGS
# we use the general approach here
LDADD = \
	-version-info $(LT_VERSION_INFO)		 		\
	-export-dynamic 						\
	-rpath $(libdir) 						\
	$(no_undefined)							\
	$(LIBTOOL_EXPORT_OPTIONS)


#
# setup source file variables
#
#
# GDK header files for public installation (non-generated)
#
gdk_public_h_sources =				\
	gdk.h					\
	gdkcolor.h				\
	gdkcursor.h				\
	gdkdisplay.h				\
	gdkdnd.h				\
	gdkdrawable.h				\
	gdkevents.h				\
	gdkfont.h				\
	gdkgc.h					\
	gdkkeysyms.h				\
	gdki18n.h				\
	gdkimage.h				\
	gdkinput.h				\
	gdkkeys.h				\
	gdkdisplaymanager.h			\
	gdkpango.h				\
	gdkpixbuf.h				\
	gdkpixmap.h				\
	gdkproperty.h				\
	gdkregion.h				\
	gdkrgb.h				\
	gdkscreen.h				\
	gdkselection.h				\
	gdkspawn.h				\
	gdktypes.h				\
	gdkvisual.h				\
	gdkwindow.h

gdk_headers =                     \
	$(gdk_public_h_sources)   \
	gdkenumtypes.h		  \
	gdkprivate.h		  \
	gdkalias.h

gdk_c_sources =                 \
	gdk.c			\
	gdkcolor.c		\
	gdkcursor.c		\
	gdkdisplay.c		\
	gdkdnd.c		\
	gdkdraw.c		\
	gdkevents.c     	\
	gdkfont.c		\
	gdkgc.c			\
	gdkglobals.c		\
	gdkkeys.c		\
	gdkkeyuni.c		\
	gdkimage.c		\
	gdkinternals.h  	\
	gdkintl.h		\
	gdkdisplaymanager.c	\
	gdkpango.c		\
	gdkpixbuf-drawable.c	\
	gdkpixbuf-render.c	\
	gdkpixmap.c		\
	gdkpoly-generic.h	\
	gdkpolyreg-generic.c	\
	gdkrgb.c		\
	gdkrectangle.c		\
	gdkregion-generic.c	\
	gdkregion-generic.h	\
	gdkscreen.c		\
	gdkselection.c		\
	gdkvisual.c		\
	gdkwindow.c

#
# setup GDK sources and their dependencies
#

gdkincludedir = $(includedir)/gtk-2.0/gdk
gdkinclude_HEADERS = $(gdk_headers)

# gdkmarshalers.c is not here becuase it is currently an empty file
common_sources =                \
	$(gdk_c_sources)	\
	gdkenumtypes.c		\
	gdkmarshalers.h

libgdk_x11_2_0_la_SOURCES = $(common_sources)
libgdk_x11_2_0_la_LIBADD = x11/libgdk-x11.la $(GDK_DEP_LIBS) \
  $(top_builddir)/gdk-pixbuf/libgdk_pixbuf-$(GTK_API_VERSION).la
libgdk_x11_2_0_la_LDFLAGS = $(LDADD)

libgdk_linux_fb_2_0_la_SOURCES = $(common_sources) gdkkeynames.c
libgdk_linux_fb_2_0_la_LIBADD = linux-fb/libgdk-linux-fb.la $(GDK_DEP_LIBS) \
  $(top_builddir)/gdk-pixbuf/libgdk_pixbuf-$(GTK_API_VERSION).la
libgdk_linux_fb_2_0_la_LDFLAGS = $(LDADD)

libgdk_win32_2_0_la_SOURCES = $(common_sources) gdkkeynames.c
libgdk_win32_2_0_la_LIBADD = win32/libgdk-win32.la $(GDK_DEP_LIBS) \
  $(top_builddir)/gdk-pixbuf/libgdk_pixbuf-$(GTK_API_VERSION).la
libgdk_win32_2_0_la_DEPENDENCIES = win32/libgdk-win32.la gdk.def
libgdk_win32_2_0_la_LDFLAGS = -export-symbols gdk.def $(LDADD)
if HAVE_WINTAB
libgdk_win32_2_0_la_LIBADD += -Lwin32 -lwntab32x
endif
if HAVE_IE55
libgdk_win32_2_0_la_LIBADD += -Lwin32 -lie55uuid
endif

if OS_WIN32
install-libtool-import-lib:
	$(INSTALL) .libs/libgdk-win32-$(GTK_API_VERSION).dll.a $(DESTDIR)$(libdir)
uninstall-libtool-import-lib:
	-rm $(DESTDIR)$(libdir)/libgdk-win32-$(GTK_API_VERSION).dll.a
else
install-libtool-import-lib:
uninstall-libtool-import-lib:
endif

if MS_LIB_AVAILABLE
noinst_DATA = gdk-win32-$(GTK_API_VERSION).lib

gdk-win32-$(GTK_API_VERSION).lib: libgdk-win32-$(GTK_API_VERSION).la gdk.def
	lib -name:libgdk-win32-$(GTK_API_VERSION)-@LT_CURRENT_MINUS_AGE@.dll -def:gdk.def -out:$@

install-ms-lib:
	$(INSTALL) gdk-win32-$(GTK_API_VERSION).lib $(DESTDIR)$(libdir)

uninstall-ms-lib:
	-rm $(DESTDIR)$(libdir)/gdk-win32-$(GTK_API_VERSION).lib
else
install-ms-lib:
uninstall-ms-lib:
endif

gdk.def: gdk.symbols
	(echo -e EXPORTS; $(CPP) -P -DGDK_WINDOWING_WIN32 -DINCLUDE_VARIABLES - <$(srcdir)/gdk.symbols | sed -e '/^$$/d' -e 's/^/	/' -e 's/G_GNUC_[^ ]*//g') > gdk.def

gdkalias.h: gdk.symbols
	 $(PERL) $(srcdir)/makegdkalias.pl < $(srcdir)/gdk.symbols > gdkalias.h

if OS_LINUX
TESTS = abicheck.sh
endif


lib_LTLIBRARIES = $(gdktargetlib)

EXTRA_LTLIBRARIES = libgdk-x11-2.0.la libgdk-linux-fb-2.0.la libgdk-win32-2.0.la

MAINTAINERCLEANFILES = gdkenumtypes.h stamp-gdkenumtypes.h gdkenumtypes.c \
  gdkmarshalers.h gdkmarshalers.c
EXTRA_HEADERS =

#
# Rule to install gdkconfig.h header file
#
configexecincludedir = $(libdir)/gtk-2.0/include
#configexecinclude_DATA = gdkconfig.h

install-exec-local: gdkconfig.h
	$(mkinstalldirs) $(DESTDIR)$(configexecincludedir)
	file=$(DESTDIR)$(configexecincludedir)/gdkconfig.h; \
	if test -r $$file && cmp -s gdkconfig.h $$file; then :; \
	else $(INSTALL_DATA) gdkconfig.h $$file; fi

install-exec-hook:
if DISABLE_EXPLICIT_DEPS
	$(SHELL) $(top_srcdir)/sanitize-la.sh $(DESTDIR)$(libdir)/$(gdktargetlib)
endif

#note: not gdkconfig.h
BUILT_SOURCES =					\
	gdkalias.h				\
	gdkenumtypes.h				\
	gdkenumtypes.c				\
	gdkmarshalers.h 			\
	gdkmarshalers.c				\
	gdkconfig.h

gdkenumtypes.h: stamp-gdkenumtypes.h
	@true
stamp-gdkenumtypes.h: @REBUILD@ $(gdk_public_h_sources) Makefile
	( cd $(srcdir) && glib-mkenums \
			--fhead "#ifndef __GDK_ENUM_TYPES_H__\n#define __GDK_ENUM_TYPES_H__\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n" \
			--fprod "/* enumerations from \"@filename@\" */\n" \
			--vhead "GType @enum_name@_get_type (void) G_GNUC_CONST;\n#define GDK_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n" \
			--ftail "G_END_DECLS\n\n#endif /* __GDK_ENUM_TYPES_H__ */" \
		$(gdk_public_h_sources) ) >> xgen-geth \
	&& (cmp -s xgen-geth gdkenumtypes.h || cp xgen-geth gdkenumtypes.h ) \
	&& rm -f xgen-geth \
	&& echo timestamp > $(@F)
gdkenumtypes.c: @REBUILD@ $(gdk_public_h_sources) Makefile
	( cd $(srcdir) && glib-mkenums \
			--fhead "#define GDK_ENABLE_BROKEN\n#include \"gdkalias.h\"\n#include \"gdk.h\"" \
		--fprod "\n/* enumerations from \"@filename@\" */" \
			--vhead "GType\n@enum_name@_get_type (void)\n{\n  static GType etype = 0;\n  if (etype == 0) {\n    static const G@Type@Value values[] = {" \
			--vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
			--vtail "      { 0, NULL, NULL }\n    };\n    etype = g_@type@_register_static (\"@EnumName@\", values);\n  }\n  return etype;\n}\n" \
		$(gdk_public_h_sources) ) > xgen-getc \
	&& cp xgen-getc gdkenumtypes.c  \
	&& rm -f xgen-getc

# 
# Marshaller generation
#
gdkmarshalers.h: @REBUILD@ gdkmarshalers.list
	$(GLIB_GENMARSHAL) --prefix=gdk_marshal $(srcdir)/gdkmarshalers.list --header > gdkmarshalers-h.tmp \
	&& mv gdkmarshalers-h.tmp gdkmarshalers.h \
	|| ( rm -f gdkmarshalers-h.tmp && exit 1)
gdkmarshalers.c: @REBUILD@ gdkmarshalers.list
	(echo "#include \"gdkalias.h\""; $(GLIB_GENMARSHAL) --prefix=gdk_marshal $(srcdir)/gdkmarshalers.list --body) > gdkmarshalers-c.tmp	\
	&& mv gdkmarshalers-c.tmp gdkmarshalers.c \
	|| ( rm -f gdkmarshalers-c.tmp && exit 1 )

gdkconfig.h: stamp-gc-h
	@if test -f gdkconfig.h; then :; \
	else rm -f stamp-gc-h; $(MAKE) stamp-gc-h; fi
stamp-gc-h: $(top_builddir)/config.status
	cd $(top_builddir) && $(SHELL) ./config.status gdk/gdkconfig.h
	echo timestamp > stamp-gc-h

DISTCLEANFILES = gdkconfig.h stamp-gc-h

install-data-local: install-ms-lib install-libtool-import-lib

uninstall-local: uninstall-ms-lib uninstall-libtool-import-lib
	rm -f $(DESTDIR)$(configexecincludedir)/gdkconfig.h

# if srcdir!=builddir, clean out maintainer-clean files from builddir
# this allows dist to pass.
distclean-local:
	if test $(srcdir) != .; then \
	  rm -f $(MAINTAINERCLEANFILES); \
	fi

.PHONY: files

files:
	@files=`ls $(DISTFILES) 2> /dev/null `; for p in $$files; do \
	  echo $$p; \
	done
