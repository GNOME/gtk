## Makefile.am for gtk+/gdk

SUBDIRS=$(gdktarget)
DIST_SUBDIRS=linux-fb win32 x11

EXTRA_DIST =			\
	gdkconfig.h.win32 	\
	gdk.def 		\
	gdkmarshalers.list	\
	makefile.mingw 		\
	makefile.mingw.in 	\
	makeenums.pl		\
	makefile.msc

common_includes = @STRIP_BEGIN@ 	\
	-DG_LOG_DOMAIN=\"Gdk\"		\
	-I$(top_srcdir)			\
	-I$(top_builddir)/gdk		\
	-I$(top_srcdir)/gdk-pixbuf 	\
	@GTK_DEBUG_FLAGS@ 		\
	@GDK_DEP_CFLAGS@		\
	-DGDK_COMPILATION		\
@STRIP_END@

INCLUDES = $(common_includes)
gtarget=@gdktarget@

if PLATFORM_WIN32
no_undefined = -no-undefined

if HAVE_WINTAB
wintab_lib = -Lwin32 -lwntab32x
endif

if HAVE_IE55
ie55uuid_lib = -Lwin32 -lie55uuid
endif
endif

if OS_WIN32
gdk_win32_symbols = -export-symbols gdk.def

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

# libtool stuff: set version and export symbols for resolving
# since automake doesn't support conditionalized libsomething_la_LDFLAGS
# we use the general approach here
LDFLAGS = @STRIP_BEGIN@ 						\
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) 		\
	-export-dynamic 						\
	-rpath $(libdir) 						\
	$(no_undefined)							\
	@LIBTOOL_EXPORT_OPTIONS@					\
	$(top_builddir)/gdk-pixbuf/libgdk_pixbuf-$(GTK_API_VERSION).la	\
	$(gdk_win32_symbols)						\
	@GDK_DEP_LIBS@							\
@STRIP_END@

#
# setup source file variables
#
#
# GDK header files for public installation (non-generated)
#
# Note: files added here may need to be be propagated to gdk_headers in gtk/Makefile.am
#
gdk_public_h_sources = @STRIP_BEGIN@		\
	gdk.h					\
	gdkcolor.h				\
	gdkcursor.h				\
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
	gdkselection.h				\
	gdktypes.h				\
	gdkvisual.h				\
	gdkwindow.h				\
	gdkdisplay.h				\
	gdkscreen.h				\
@STRIP_END@

gdk_headers = @STRIP_BEGIN@       \
	$(gdk_public_h_sources)   \
	gdkenumtypes.h		  \
	gdkprivate.h		  \
@STRIP_END@

gdk_c_sources = @STRIP_BEGIN@ 	\
	gdk.c			\
	gdkcolor.c		\
	gdkcursor.c		\
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
	gdkselection.c		\
	gdkvisual.c		\
	gdkwindow.c		\
	gdkdisplay.c		\
	gdkscreen.c		\
@STRIP_END@

#
# setup GDK sources and their dependancies
#

gdkincludedir = $(includedir)/gtk-2.0/gdk
gdkinclude_HEADERS = $(gdk_headers)

# gdkmarshalers.c is commented out becuase it is currently an empty file
libgdk_x11_2_0_la_SOURCES = $(gdk_c_sources) gdkenumtypes.c gdkmarshalers.h # gdkmarshalers.c
libgdk_linux_fb_2_0_la_SOURCES = $(gdk_c_sources) gdkenumtypes.c gdkmarshalers.h # gdkmarshalers.c
libgdk_win32_2_0_la_SOURCES = $(gdk_c_sources) gdkenumtypes.c # gdkmarshalers.c

libgdk_x11_2_0_la_LIBADD = x11/libgdk-x11.la	
libgdk_linux_fb_2_0_la_LIBADD = linux-fb/libgdk-linux-fb.la	
libgdk_win32_2_0_la_LIBADD = \
	win32/libgdk-win32.la $(wintab_lib) $(ie55uuid_lib)

lib_LTLIBRARIES = $(gdktargetlib)

EXTRA_LTLIBRARIES = libgdk-x11-2.0.la libgdk-linux-fb-2.0.la libgdk-win32-2.0.la

MAINTAINERCLEANFILES = gdkenumtypes.h stamp-gdkenumtypes.h
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
BUILT_SOURCES = stamp-gc-h

# Generate built header without using automake-1.4 BUILT_SOURCES
$(libgdk_x11_2_0_la_OBJECTS) $(libgdk_linux_fb_2_0_la_OBJECTS) $(libgdk_win32_2_0_la_OBJECTS): gdkenumtypes.h gdkmarshalers.h

$(srcdir)/gdkenumtypes.h: stamp-gdkenumtypes.h
	@true
stamp-gdkenumtypes.h: @REBUILD@ $(gdk_public_h_sources) Makefile
	( cd $(srcdir) && glib-mkenums \
			--fhead "#ifndef __GDK_ENUM_TYPES_H__\n#define __GDK_ENUM_TYPES_H__\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n" \
			--fprod "/* enumerations from \"@filename@\" */\n" \
			--vhead "GType @enum_name@_get_type (void);\n#define GDK_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n" \
			--ftail "G_END_DECLS\n\n#endif /* __GDK_ENUM_TYPES_H__ */" \
		$(gdk_public_h_sources) ) >> xgen-geth \
	&& (cmp -s xgen-geth $(srcdir)/gdkenumtypes.h || cp xgen-geth $(srcdir)/gdkenumtypes.h ) \
	&& rm -f xgen-geth \
	&& echo timestamp > $(@F)
$(srcdir)/gdkenumtypes.c: @REBUILD@ $(gdk_public_h_sources) Makefile
	( cd $(srcdir) && glib-mkenums \
			--fhead "#define GDK_ENABLE_BROKEN\n#include \"gdk.h\"" \
		--fprod "\n/* enumerations from \"@filename@\" */" \
			--vhead "GType\n@enum_name@_get_type (void)\n{\n  static GType etype = 0;\n  if (etype == 0) {\n    static const G@Type@Value values[] = {" \
			--vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
			--vtail "      { 0, NULL, NULL }\n    };\n    etype = g_@type@_register_static (\"@EnumName@\", values);\n  }\n  return etype;\n}\n" \
		$(gdk_public_h_sources) ) > xgen-getc \
	&& cp xgen-getc $(srcdir)/gdkenumtypes.c  \
	&& rm -f xgen-getc

# 
# Marshaller generation
#
$(srcdir)/gdkmarshalers.h: @REBUILD@ gdkmarshalers.list
	cd $(srcdir) &&											\
        ( @GLIB_GENMARSHAL@ --prefix=gdk_marshal gdkmarshalers.list --header > gdkmarshalers.tmp	\
	&& mv gdkmarshalers.tmp gdkmarshalers.h )							\
        || ( rm -f gdkmarshalers.tmp && exit 1 )
$(srcdir)/gdkmarshalers.c: @REBUILD@ gdkmarshalers.list
	cd $(srcdir) &&											\
	$( @GLIB_GENMARSHAL@ --prefix=gdk_marshal gdkmarshalers.list --body > gdkmarshalers.tmp	\
	&& mv gdkmarshalers.tmp gdkmarshalers.c )							\
       || ( rm -f gdkmarshalers.tmp && exit 1 )

gdkconfig.h: stamp-gc-h
	@if test -f gdkconfig.h; then :; \
	else rm -f stamp-gc-h; $(MAKE) stamp-gc-h; fi
stamp-gc-h: ../config.status
	cd .. && CONFIG_FILES= CONFIG_HEADERS= CONFIG_OTHER=gdk/gdkconfig.h ./config.status
	echo timestamp > stamp-gc-h

install-data-local: install-ms-lib install-libtool-import-lib

uninstall-local: uninstall-ms-lib uninstall-libtool-import-lib

.PHONY: files

files:
	@files=`ls $(DISTFILES) 2> /dev/null `; for p in $$files; do \
	  echo $$p; \
	done
