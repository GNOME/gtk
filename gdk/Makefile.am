## Makefile.am for gtk+/gdk
include $(top_srcdir)/Makefile.decl
include gdk-sources.inc

-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ENV = \
   CC="$(CC)"
INTROSPECTION_SCANNER_ARGS = \
	--add-include-path=../gdk \
	--warn-all
INTROSPECTION_COMPILER_ARGS = \
   --includedir=$(srcdir) \
   --includedir=.

SUBDIRS = $(GDK_BACKENDS) .

DIST_SUBDIRS = win32 x11 quartz broadway wayland

CLEANFILES =

EXTRA_DIST += 			\
	keynames.txt		\
	keynames-translate.txt	\
	keyname-table.h		\
	gdkkeynames.c		\
	gen-keyname-table.pl	\
	gdkconfig.h.win32	\
	gdkconfig.h.win32_broadway	\
	gdkkeysyms-update.pl	\
	gdkmarshalers.list	\
	gdkwindowimpl.h		\
	makefile.msc		\
	gdkenumtypes.c.template	\
	gdkenumtypes.h.template	\
	gdkversionmacros.h.in	\
	meson.build \
	gen-gdk-gresources-xml.py \
	gdkconfig.h.meson

GDK_CFLAGS_DEFINES =	\
	-DG_LOG_USE_STRUCTURED=1	\
	-DGDK_COMPILATION


AM_CPPFLAGS =				\
	-DG_LOG_DOMAIN=\"Gdk\"		\
	$(GDK_CFLAGS_DEFINES)		\
	-I$(top_builddir)		\
	-I$(top_builddir)/gdk		\
	-I$(top_srcdir)			\
	$(GTK_DEBUG_FLAGS)		\
	$(GDK_DEP_CFLAGS)

# libtool stuff: set version and export symbols for resolving
# since automake doesn't support conditionalized libsomething_la_LDFLAGS
# we use the general approach here
LDADD = 					\
	$(GTK_LINK_FLAGS)			\
	-version-info $(LT_VERSION_INFO)	\
	-export-dynamic 			\
	-rpath $(libdir) 			\
	$(no_undefined)

#
# setup GDK sources and their dependencies
#

gdkincludedir = $(includedir)/gtk-3.0/gdk
gdkinclude_HEADERS = $(gdk_public_h_sources)
nodist_gdkinclude_HEADERS = gdkconfig.h gdkenumtypes.h gdkversionmacros.h

deprecatedincludedir = $(includedir)/gtk-3.0/gdk/deprecated
deprecatedinclude_HEADERS = $(gdk_deprecated_h_sources)

common_sources = 		\
	$(gdk_private_headers)	\
	$(gdk_c_sources)	\
	gdkenumtypes.c		\
	gdkmarshalers.c		\
	gdkmarshalers.h		\
	gdkresources.h		\
	gdkresources.c

libgdk_3_la_SOURCES = $(common_sources)
libgdk_3_la_CFLAGS = $(AM_CFLAGS) $(GDK_HIDDEN_VISIBILITY_CFLAGS)
libgdk_3_la_LIBADD = $(GDK_DEP_LIBS) $(SHM_LIBS)
libgdk_3_la_LDFLAGS = $(LDADD)

if USE_X11
libgdk_3_la_LIBADD += x11/libgdk-x11.la
endif # USE_X11

if USE_QUARTZ
libgdk_3_la_LIBADD += quartz/libgdk-quartz.la
endif # USE_QUARTZ

if USE_WIN32
libgdk_3_la_LIBADD += win32/libgdk-win32.la
libgdk_3_la_DEPENDENCIES = win32/libgdk-win32.la win32/rc/gdk-win32-res.o
libgdk_3_la_LDFLAGS += -Wl,win32/rc/gdk-win32-res.o
endif # USE_WIN32

if USE_BROADWAY
libgdk_3_la_LIBADD += broadway/libgdk-broadway.la
endif # USE_BROADWAY

if USE_WAYLAND
libgdk_3_la_LIBADD += wayland/libgdk-wayland.la
endif

if HAVE_INTROSPECTION

introspection_files = 		\
	$(filter-out gdkkeysyms-compat.h, $(gdk_h_sources))	\
	$(gdk_c_sources)	\
	$(gdk_built_sources)

Gdk-3.0.gir: libgdk-3.la Makefile
Gdk_3_0_gir_SCANNERFLAGS = 	\
	--c-include="gdk/gdk.h"
Gdk_3_0_gir_INCLUDES = Gio-2.0 GdkPixbuf-2.0 Pango-1.0 cairo-1.0
Gdk_3_0_gir_LIBS = libgdk-3.la
Gdk_3_0_gir_FILES = $(introspection_files)
Gdk_3_0_gir_CFLAGS = $(AM_CPPFLAGS)
Gdk_3_0_gir_EXPORT_PACKAGES = gdk-3.0
INTROSPECTION_GIRS += Gdk-3.0.gir
if OS_WIN32
Gdk_3_0_gir_CFLAGS += -I$(srcdir)/win32 -I$(srcdir)
endif
if USE_X11
GdkX11-3.0.gir: libgdk-3.la Gdk-3.0.gir Makefile
GdkX11_3_0_gir_SCANNERFLAGS = 		\
	--identifier-prefix=Gdk		\
	--c-include="gdk/gdkx.h"	\
	--include-uninstalled=$(top_builddir)/gdk/Gdk-3.0.gir
GdkX11_3_0_gir_INCLUDES = Gio-2.0 GdkPixbuf-2.0 Pango-1.0 xlib-2.0
GdkX11_3_0_gir_LIBS = libgdk-3.la
GdkX11_3_0_gir_FILES = $(x11_introspection_files)
GdkX11_3_0_gir_CFLAGS = $(AM_CPPFLAGS) -L$(top_builddir)/gdk
GdkX11_3_0_gir_EXPORT_PACKAGES = gdk-x11-3.0
INTROSPECTION_GIRS += GdkX11-3.0.gir

endif # USE_X11

if OS_WIN32
GdkWin32-3.0.gir: libgdk-3.la Gdk-3.0.gir Makefile
GdkWin32_3_0_gir_SCANNERFLAGS = \
	--identifier-prefix=Gdk		\
	--c-include="gdk/gdkwin32.h"	\
	--include-uninstalled=$(top_builddir)/gdk/Gdk-3.0.gir
GdkWin32_3_0_gir_INCLUDES = Gio-2.0 GdkPixbuf-2.0 Pango-1.0
GdkWin32_3_0_gir_LIBS = libgdk-3.la
GdkWin32_3_0_gir_FILES = $(w32_introspection_files)
GdkWin32_3_0_gir_CFLAGS = $(AM_CPPFLAGS) -L$(top_builddir)/gdk -I$(srcdir)/win32 -I$(srcdir)
INTROSPECTION_GIRS += GdkWin32-3.0.gir

endif # OS_WIN32

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibsdir = $(libdir)/girepository-1.0
typelibs_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelibs_DATA)

endif # HAVE_INTROSPECTION

if OS_WIN32
gdk.def: libgdk-3.la
	echo "LIBRARY libgdk-$(GTK_MAJOR_VERSION)-@LT_CURRENT_MINUS_AGE@" >$@
	echo "EXPORTS" >>$@
	objdump -p .libs/libgdk-$(GTK_MAJOR_VERSION)-@LT_CURRENT_MINUS_AGE@.dll | sed -n -e 's/\t\[\s\+\([0-9]\+\)\]\s\([A-Za-z_][A-Za-z0-9_]\)/\2/p' >>$@
install-def-file: gdk.def
	mkdir -p $(DESTDIR)$(libdir)
	$(INSTALL) gdk.def $(DESTDIR)$(libdir)/gdk-win32-3.0.def
uninstall-def-file:
	-rm $(DESTDIR)$(libdir)/gdk-win32-3.0.def
else
install-def-file:
uninstall-def-file:
endif

if MS_LIB_AVAILABLE
noinst_DATA = gdk-win32-$(GTK_API_VERSION).lib

gdk-win32-$(GTK_API_VERSION).lib: libgdk-3.la gdk.def
	lib -machine:@LIB_EXE_MACHINE_FLAG@ -name:libgdk-win32-$(GTK_API_VERSION)-@LT_CURRENT_MINUS_AGE@.dll -def:gdk.def -out:$@

install-ms-lib:
	mkdir -p $(DESTDIR)$(libdir)
	$(INSTALL) gdk-win32-$(GTK_API_VERSION).lib $(DESTDIR)$(libdir)

uninstall-ms-lib:
	-rm $(DESTDIR)$(libdir)/gdk-win32-$(GTK_API_VERSION).lib
else
install-ms-lib:
uninstall-ms-lib:
endif

lib_LTLIBRARIES = libgdk-3.la

MAINTAINERCLEANFILES = $(gdk_built_sources) stamp-gdkenumtypes.h
EXTRA_DIST += \
	fallback-c89.c

install-exec-hook:
if DISABLE_EXPLICIT_DEPS
	$(SHELL) $(top_srcdir)/sanitize-la.sh $(DESTDIR)$(libdir)/libgdk-3.la
endif

BUILT_SOURCES = \
	$(gdk_built_sources)			\
	gdkconfig.h

gdkenumtypes.h: stamp-gdkenumtypes.h
	@true
stamp-gdkenumtypes.h: $(gdk_h_sources) gdkenumtypes.h.template
	$(AM_V_GEN) ( cd $(srcdir) && $(GLIB_MKENUMS) --template gdkenumtypes.h.template \
		$(gdk_h_sources) ) >> xgen-geth \
	&& (cmp -s xgen-geth gdkenumtypes.h || cp xgen-geth gdkenumtypes.h ) \
	&& rm -f xgen-geth \
	&& echo timestamp > $(@F)
gdkenumtypes.c: $(gdk_h_sources) gdkenumtypes.c.template
	$(AM_V_GEN) ( cd $(srcdir) && $(GLIB_MKENUMS) --template gdkenumtypes.c.template \
		$(gdk_h_sources) ) > xgen-getc \
	&& cp xgen-getc gdkenumtypes.c  \
	&& rm -f xgen-getc

#
# Marshaller generation
#
gdkmarshalers.h: gdkmarshalers.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) \
		--prefix=_gdk_marshal \
		--valist-marshallers \
		--header \
		$(srcdir)/gdkmarshalers.list > gdkmarshalers-h.tmp \
	&& mv gdkmarshalers-h.tmp gdkmarshalers.h \
	|| ( rm -f gdkmarshalers-h.tmp && exit 1)
gdkmarshalers.c: gdkmarshalers.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) \
		--prefix=_gdk_marshal \
		--valist-marshallers \
		--body \
		$(srcdir)/gdkmarshalers.list > gdkmarshalers-c.tmp	\
	&& mv gdkmarshalers-c.tmp gdkmarshalers.c \
	|| ( rm -f gdkmarshalers-c.tmp && exit 1 )

gdkconfig.h: stamp-gc-h
	$(AM_V_GEN) if test -f gdkconfig.h; then :; \
	else rm -f stamp-gc-h; $(MAKE) stamp-gc-h; fi
stamp-gc-h: $(top_builddir)/config.status
	$(AM_V_at) cd $(top_builddir) && $(SHELL) ./config.status gdk/gdkconfig.h \
	&& echo timestamp > gdk/$(@F)

#
# Resources
#

glsl_sources := $(sort $(wildcard $(srcdir)/resources/glsl/*.glsl))

gdk.gresource.xml: Makefile.am
	$(AM_V_GEN) echo "<?xml version='1.0' encoding='UTF-8'?>" > $@; \
	echo "<gresources>" >> $@; \
	echo "  <gresource prefix='/org/gtk/libgdk'>" >> $@; \
	for f in $(glsl_sources); do \
	  n=`basename $$f`; \
	  echo "    <file alias='glsl/$$n'>resources/glsl/$$n</file>" >> $@; \
	done; \
	echo "  </gresource>" >> $@; \
	echo "</gresources>" >> $@;

EXTRA_DIST += $(glsl_sources)
CLEANFILES += gdk.gresource.xml

resource_files = $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(builddir)/gdk.gresource.xml)

gdkresources.h: gdk.gresource.xml
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $< --target=$@ \
		--sourcedir=$(srcdir) --c-name _gdk --generate-header --manual-register --internal
gdkresources.c: gdk.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $< --target=$@ \
		--sourcedir=$(srcdir) --c-name _gdk --generate-source --manual-register --internal

# ------------------- MSVC Build Items ----------------
MSVCPROJS = gdk-3

gdk_3_FILES = $(gdk_c_sources)
gdk_3_EXCLUDES = dummy

include $(top_srcdir)/win32/Makefile.msvcproj

dist-hook: \
	$(top_builddir)/win32/vs9/gdk-3.vcproj

DISTCLEANFILES = gdkconfig.h stamp-gc-h

install-data-local: install-ms-lib install-def-file

uninstall-local: uninstall-ms-lib uninstall-def-file
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

-include $(top_srcdir)/git.mk
