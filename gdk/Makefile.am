## Makefile.am for gtk+/gdk
include $(top_srcdir)/Makefile.decl
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

DIST_SUBDIRS = win32 x11 quartz broadway wayland mir

CLEANFILES =

EXTRA_DIST += 			\
	keynames.txt		\
	keynames-translate.txt	\
	keyname-table.h		\
	gdkkeynames.c		\
	gen-keyname-table.pl	\
	gdkconfig.h.win32	\
	gdkconfig.h.win32_vulkan	\
	gdkkeysyms-update.pl	\
	gdkmarshalers.list	\
	gdkwindowimpl.h		\
	makefile.msc		\
	gdkenumtypes.c.template	\
	gdkenumtypes.h.template	\
	gdkversionmacros.h.in

GDK_CFLAGS_DEFINES =	\
	-DG_LOG_DOMAIN=\"Gdk\"		\
	-DG_LOG_USE_STRUCTURED=1	\
	-DGDK_COMPILATION


AM_CPPFLAGS =				\
	$(GDK_CFLAGS_DEFINES)		\
	-I$(top_builddir)		\
	-I$(top_builddir)/gdk		\
	-I$(top_srcdir)			\
	$(GTK_DEBUG_FLAGS)		\
	$(GDK_DEP_CFLAGS)

#
# setup source file variables
#
#
# GDK header files for public installation (non-generated)
#
#
gdk_public_h_sources = 				\
	gdk.h					\
	gdk-autocleanup.h			\
	gdkapplaunchcontext.h			\
	gdkcairo.h				\
	gdkcursor.h				\
	gdkdevice.h				\
	gdkdevicepad.h				\
	gdkdevicetool.h				\
	gdkdevicemanager.h			\
	gdkdisplay.h				\
	gdkdisplaymanager.h			\
	gdkdnd.h				\
	gdkdrawcontext.h			\
	gdkdrawingcontext.h			\
	gdkevents.h				\
	gdkframetimings.h			\
	gdkglcontext.h				\
	gdkkeys.h				\
	gdkkeysyms.h				\
	gdkkeysyms-compat.h			\
	gdkmain.h				\
	gdkmonitor.h				\
	gdkpango.h				\
	gdkframeclock.h				\
	gdkpixbuf.h				\
	gdkprivate.h				\
	gdkproperty.h				\
	gdkrectangle.h				\
	gdkrgba.h				\
	gdkscreen.h				\
	gdkseat.h				\
	gdkselection.h				\
	gdkthreads.h				\
	gdktypes.h				\
	gdkvisual.h				\
	gdkvulkancontext.h			\
	gdkwindow.h

gdk_h_sources =					\
	$(gdk_public_h_sources)

gdk_private_headers = 				\
	gdk-private.h				\
	gdkapplaunchcontextprivate.h		\
	gdkcursorprivate.h			\
	gdkdevicemanagerprivate.h		\
	gdkdeviceprivate.h			\
	gdkdevicepadprivate.h			\
	gdkdevicetoolprivate.h			\
	gdkdisplaymanagerprivate.h		\
	gdkdisplayprivate.h			\
	gdkdndprivate.h				\
	gdkdrawcontextprivate.h			\
	gdkdrawingcontextprivate.h		\
	gdkframeclockidle.h			\
	gdkframeclockprivate.h			\
	gdkglcontextprivate.h			\
	gdkmonitorprivate.h			\
	gdkscreenprivate.h			\
	gdkseatprivate.h			\
	gdkseatdefaultprivate.h			\
	gdkinternals.h				\
	gdkintl.h				\
	gdkkeysprivate.h			\
	gdkvisualprivate.h			\
	gdkvulkancontextprivate.h		\
	gdkx.h

gdk_c_sources = 				\
	gdk.c					\
	gdkapplaunchcontext.c			\
	gdkcairo.c				\
	gdkcursor.c				\
	gdkdeprecated.c				\
	gdkdevice.c				\
	gdkdevicepad.c				\
	gdkdevicetool.c				\
	gdkdevicemanager.c			\
	gdkdisplay.c				\
	gdkdisplaymanager.c			\
	gdkdnd.c				\
	gdkdrawcontext.c			\
	gdkdrawingcontext.c			\
	gdkevents.c     			\
	gdkframetimings.c			\
	gdkgl.c					\
	gdkglcontext.c				\
	gdkglobals.c				\
	gdkkeys.c				\
	gdkkeyuni.c				\
	gdkmonitor.c				\
	gdkframeclock.c				\
	gdkframeclockidle.c			\
	gdkpango.c				\
	gdkpixbuf-drawable.c			\
	gdkproperty.c				\
	gdkrectangle.c				\
	gdkrgba.c				\
	gdkscreen.c				\
	gdkseat.c				\
	gdkseatdefault.c			\
	gdkselection.c				\
	gdkvisual.c				\
	gdkvulkancontext.c			\
	gdkwindow.c				\
	gdkwindowimpl.c

gdk_built_sources = 				\
	gdkenumtypes.h				\
	gdkenumtypes.c				\
	gdkmarshalers.h 			\
	gdkmarshalers.c				\
	gdkresources.h				\
	gdkresources.c				\
	gdkversionmacros.h

#
# setup GDK sources and their dependencies
#

gdkincludedir = $(includedir)/gtk-4.0/gdk
gdkinclude_HEADERS = $(gdk_public_h_sources)
nodist_gdkinclude_HEADERS = gdkconfig.h gdkenumtypes.h gdkversionmacros.h

deprecatedincludedir = $(includedir)/gtk-4.0/gdk/deprecated
deprecatedinclude_HEADERS = $(deprecated_h_sources)

common_sources = 		\
	$(gdk_private_headers)	\
	$(gdk_c_sources)	\
	gdkenumtypes.c		\
	gdkmarshalers.c		\
	gdkmarshalers.h		\
	gdkresources.h		\
	gdkresources.c

libgdk_4_la_SOURCES = $(common_sources)
libgdk_4_la_CFLAGS = $(AM_CFLAGS) $(GTK_WARN_CFLAGS) $(GDK_HIDDEN_VISIBILITY_CFLAGS)
libgdk_4_la_LIBADD = $(GDK_DEP_LIBS) $(SHM_LIBS)
libgdk_4_la_LDFLAGS = $(LDADD)

if USE_X11
libgdk_4_la_LIBADD += x11/libgdk-x11.la
endif # USE_X11

if USE_QUARTZ
libgdk_4_la_CFLAGS += -xobjective-c
libgdk_4_la_LIBADD += quartz/libgdk-quartz.la
endif # USE_QUARTZ

if USE_WIN32
libgdk_4_la_LIBADD += win32/libgdk-win32.la
endif # USE_WIN32

if USE_BROADWAY
libgdk_4_la_LIBADD += broadway/libgdk-broadway.la
endif # USE_BROADWAY

if USE_WAYLAND
libgdk_4_la_LIBADD += wayland/libgdk-wayland.la
endif

if USE_MIR
libgdk_4_la_LIBADD += mir/libgdk-mir.la
endif

if HAVE_INTROSPECTION

introspection_files = 		\
	$(filter-out gdkkeysyms-compat.h, $(gdk_h_sources))	\
	$(gdk_c_sources)	\
        gdkenumtypes.c		\
        gdkenumtypes.h

Gdk-noinst-4.0.gir: libgdk-4.la Makefile
Gdk_noinst_4_0_gir_SCANNERFLAGS = 	\
	--c-include="gdk/gdk.h"
Gdk_noinst_4_0_gir_INCLUDES = Gio-2.0 GdkPixbuf-2.0 Pango-1.0 cairo-1.0
Gdk_noinst_4_0_gir_LIBS = libgdk-4.la
Gdk_noinst_4_0_gir_FILES = $(introspection_files)
Gdk_noinst_4_0_gir_CFLAGS = $(AM_CPPFLAGS)
Gdk_noinst_4_0_gir_EXPORT_PACKAGES = gdk-4.0
INTROSPECTION_GIRS += Gdk-noinst-4.0.gir
if OS_WIN32
Gdk_noinst_4_0_gir_CFLAGS += -I$(srcdir)/win32 -I$(srcdir)
endif
if USE_X11
x11_introspection_files = 		\
	x11/gdkapplaunchcontext-x11.c	\
	x11/gdkasync.c			\
	x11/gdkcursor-x11.c		\
	x11/gdkdevice-core-x11.c	\
	x11/gdkdevicemanager-core-x11.c	\
	x11/gdkdevicemanager-x11.c	\
	x11/gdkdevicemanager-xi2.c	\
	x11/gdkdevice-xi2.c		\
	x11/gdkdisplay-x11.c		\
	x11/gdkdisplaymanager-x11.c	\
	x11/gdkdnd-x11.c		\
	x11/gdkeventsource.c		\
	x11/gdkeventtranslator.c	\
	x11/gdkgeometry-x11.c		\
	x11/gdkkeys-x11.c		\
	x11/gdkmain-x11.c		\
	x11/gdkproperty-x11.c		\
	x11/gdkscreen-x11.c		\
	x11/gdkselection-x11.c		\
	x11/gdksettings.c		\
	x11/gdkvisual-x11.c		\
	x11/gdkwindow-x11.c		\
	x11/gdkxftdefaults.c		\
	x11/gdkxid.c			\
	x11/xsettings-client.c		\
	x11/gdkx.h			\
	x11/gdkx11applaunchcontext.h	\
	x11/gdkx11cursor.h		\
	x11/gdkx11device.h		\
	x11/gdkx11device-core.h		\
	x11/gdkx11device-xi2.h		\
	x11/gdkx11devicemanager.h	\
	x11/gdkx11devicemanager-core.h	\
	x11/gdkx11devicemanager-xi2.h	\
	x11/gdkx11display.h		\
	x11/gdkx11displaymanager.h	\
	x11/gdkx11dnd.h			\
	x11/gdkx11keys.h		\
	x11/gdkx11property.h		\
	x11/gdkx11screen.h		\
	x11/gdkx11selection.h		\
	x11/gdkx11utils.h		\
	x11/gdkx11visual.h		\
	x11/gdkx11window.h

GdkX11-noinst-4.0.gir: libgdk-4.la Gdk-noinst-4.0.gir Makefile
GdkX11_noinst_4_0_gir_SCANNERFLAGS = 		\
	--identifier-prefix=Gdk		\
	--c-include="gdk/gdkx.h"	\
	--include-uninstalled=$(top_builddir)/gdk/Gdk-noinst-4.0.gir
GdkX11_noinst_4_0_gir_INCLUDES = Gio-2.0 GdkPixbuf-2.0 Pango-1.0 xlib-2.0
GdkX11_noinst_4_0_gir_LIBS = libgdk-4.la
GdkX11_noinst_4_0_gir_FILES = $(x11_introspection_files)
GdkX11_noinst_4_0_gir_CFLAGS = $(AM_CPPFLAGS) -L$(top_builddir)/gdk
GdkX11_noinst_4_0_gir_EXPORT_PACKAGES = gdk-x11-4.0
INTROSPECTION_GIRS += GdkX11-noinst-4.0.gir

endif # USE_X11

w32_introspection_files =		\
	win32/gdkcursor-win32.c		\
	win32/gdkdevicemanager-win32.c	\
	win32/gdkdevice-virtual.c	\
	win32/gdkdevice-win32.c		\
	win32/gdkdevice-wintab.c	\
	win32/gdkdisplaymanager-win32.c	\
	win32/gdkdisplay-win32.c	\
	win32/gdkdnd-win32.c		\
	win32/gdkevents-win32.c		\
	win32/gdkgeometry-win32.c	\
	win32/gdkglobals-win32.c	\
	win32/gdkkeys-win32.c		\
	win32/gdkmain-win32.c		\
	win32/gdkproperty-win32.c	\
	win32/gdkscreen-win32.c		\
	win32/gdkselection-win32.c	\
	win32/gdkwin32.h		\
	win32/gdkwin32cursor.h		\
	win32/gdkwin32display.h		\
	win32/gdkwin32displaymanager.h	\
	win32/gdkwin32dnd.h		\
	win32/gdkwin32id.c		\
	win32/gdkwin32keys.h		\
	win32/gdkwin32misc.h		\
	win32/gdkwin32screen.h		\
	win32/gdkwin32window.h		\
	win32/gdkwindow-win32.c

if OS_WIN32
GdkWin32-noinst-4.0.gir: libgdk-4.la Gdk-noinst-4.0.gir Makefile
GdkWin32_noinst_4_0_gir_SCANNERFLAGS = \
	--identifier-prefix=Gdk		\
	--c-include="gdk/gdkwin32.h"	\
	--include-uninstalled=$(top_builddir)/gdk/Gdk-noinst-4.0.gir
GdkWin32_noinst_4_0_gir_INCLUDES = Gio-2.0 GdkPixbuf-2.0 Pango-1.0
GdkWin32_noinst_4_0_gir_LIBS = libgdk-4.la
GdkWin32_noinst_4_0_gir_FILES = $(w32_introspection_files)
GdkWin32_noinst_4_0_gir_CFLAGS = $(AM_CPPFLAGS) -L$(top_builddir)/gdk -I$(srcdir)/win32 -I$(srcdir)
INTROSPECTION_GIRS += GdkWin32-noinst-4.0.gir

endif # OS_WIN32

noinst_DATA = $(INTROSPECTION_GIRS)

CLEANFILES += $(INTROSPECTION_GIRS)

endif # HAVE_INTROSPECTION

if OS_WIN32
gdk.def: libgdk-4.la
	echo "LIBRARY libgdk-$(GTK_MAJOR_VERSION)-@LT_CURRENT_MINUS_AGE@" >$@
	echo "EXPORTS" >>$@
	objdump -p .libs/libgdk-$(GTK_MAJOR_VERSION)-@LT_CURRENT_MINUS_AGE@.dll | sed -n -e 's/\t\[\s\+\([0-9]\+\)\]\s\([A-Za-z_][A-Za-z0-9_]\)/\2/p' >>$@
install-def-file: gdk.def
	mkdir -p $(DESTDIR)$(libdir)
	$(INSTALL) gdk.def $(DESTDIR)$(libdir)/gdk-win32-4.0.def
uninstall-def-file:
	-rm $(DESTDIR)$(libdir)/gdk-win32-4.0.def
else
install-def-file:
uninstall-def-file:
endif

if MS_LIB_AVAILABLE
noinst_DATA = gdk-win32-$(GTK_API_VERSION).lib

gdk-win32-$(GTK_API_VERSION).lib: libgdk-4.la gdk.def
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

noinst_LTLIBRARIES = libgdk-4.la

MAINTAINERCLEANFILES = $(gdk_built_sources) stamp-gdkenumtypes.h
EXTRA_DIST += \
	$(gdk_built_sources)	\
	fallback-c89.c

install-exec-hook:
if DISABLE_EXPLICIT_DEPS
	$(SHELL) $(top_srcdir)/sanitize-la.sh $(DESTDIR)$(libdir)/libgdk-4.la
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
		--header \
		$(srcdir)/gdkmarshalers.list > gdkmarshalers-h.tmp \
	&& mv gdkmarshalers-h.tmp gdkmarshalers.h \
	|| ( rm -f gdkmarshalers-h.tmp && exit 1)
gdkmarshalers.c: gdkmarshalers.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) \
		--prefix=_gdk_marshal \
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

glsl_sources := $(wildcard $(srcdir)/resources/glsl/*.glsl)

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
		--sourcedir=$(srcdir) --c-name _gdk --generate-header --manual-register
gdkresources.c: gdk.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $< --target=$@ \
		--sourcedir=$(srcdir) --c-name _gdk --generate-source --manual-register

# ------------------- MSVC Build Items ----------------
MSVCPROJS = gdk-4

gdk_4_FILES = $(libgdk_4_la_SOURCES)
gdk_4_EXCLUDES = dummy
gdk_4_HEADERS_DIR = $(gdkincludedir)

gdk_4_HEADERS_INST = \
	$(gdkinclude_HEADERS)		\
	$(deprecatedinclude_HEADERS)	\
	$(nodist_gdkinclude_HEADERS)

gdk_4_HEADERS_EXCLUDES = dummy

include $(top_srcdir)/win32/Makefile.msvcproj

if HAVE_INTROSPECTION
# Introspection Items for MSVC
MSVC_INTROSPECT_GIRS = Gdk-4.0.gir GdkWin32-4.0.gir

BASE_MSVC_GIR_CFLAGS =			\
	$(GDK_CFLAGS_DEFINES)		\
	-I../.. -I../../gdk -I.../../gdk/win32

INTROSPECTION_INTERMEDIATE_ITEMS = \
	$(top_builddir)/win32/Gdk-4.0.gir.msvc.introspect	\
	$(top_builddir)/win32/Gdk_4_0_gir_list			\
	$(top_builddir)/win32/GdkWin32-4.0.gir.msvc.introspect	\
	$(top_builddir)/win32/GdkWin32_4_0_gir_list

Gdk_4_0_gir_MSVC_FILES = $(introspection_files)
Gdk_4_0_gir_MSVC_EXPORT_PACKAGES = $(Gdk_noinst_4_0_gir_EXPORT_PACKAGES)
Gdk_4_0_gir_MSVC_INCLUDE_GIRS = $(Gdk_noinst_4_0_gir_INCLUDES)
Gdk_4_0_gir_MSVC_LIBS = gtk-4
Gdk_4_0_gir_MSVC_CFLAGS = $(BASE_MSVC_GIR_CFLAGS)
Gdk_4_0_gir_MSVC_SCANNERFLAGS = $(Gdk_noinst_4_0_gir_SCANNERFLAGS)

GdkWin32_4_0_gir_MSVC_FILES = $(w32_introspection_files)
GdkWin32_4_0_gir_MSVC_INCLUDE_GIRS = $(GdkWin32_noinst_4_0_gir_INCLUDES)
GdkWin32_4_0_gir_MSVC_LIBS = $(Gdk_4_0_gir_MSVC_LIBS)
GdkWin32_4_0_gir_MSVC_CFLAGS = $(BASE_MSVC_GIR_CFLAGS)
GdkWin32_4_0_gir_MSVC_SCANNERFLAGS =	\
	--identifier-prefix=Gdk		\
	--c-include="gdk/gdkwin32.h"	\
	--include-uninstalled=./Gdk-4.0.gir

include $(top_srcdir)/win32/Makefile.msvc-introspection

else
INTROSPECTION_INTERMEDIATE_ITEMS =
endif

dist-hook: \
	$(top_builddir)/win32/vs12/gdk-4.vcxproj	\
	$(top_builddir)/win32/vs12/gdk-4.vs12.headers	\
	$(INTROSPECTION_INTERMEDIATE_ITEMS)

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
