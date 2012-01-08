## Makefile.am for gtk+/gdk
include $(top_srcdir)/Makefile.decl
-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = \
   --add-include-path=../gdk
INTROSPECTION_COMPILER_ARGS = \
   --includedir=$(srcdir) \
   --includedir=.

SUBDIRS = $(gdktarget) . tests
DIST_SUBDIRS = win32 x11 quartz directfb tests

CLEANFILES =

EXTRA_DIST +=			\
	keynames.txt		\
	keyname-table.h		\
	gen-keyname-table.pl	\
	gdkconfig.h.win32 	\
	gdkkeysyms-update.pl	\
	gdk.def 		\
	gdkmarshalers.list	\
	gdkmedialib.h		\
	gdkwindowimpl.h		\
	makeenums.pl		\
	makefile.msc		\
	gdk.symbols		\
	makegdkalias.pl		\
	gdkenumtypes.c.template	\
	gdkenumtypes.h.template	\
	abicheck.sh		\
	pltcheck.sh

INCLUDES =                              \
	-DG_LOG_DOMAIN=\"Gdk\"		\
	-DGDK_COMPILATION		\
	-I$(top_srcdir)			\
	-I$(top_builddir)		\
	-I$(top_builddir)/gdk		\
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
	$(GTK_LINK_FLAGS)						\
	-version-info $(LT_VERSION_INFO)		 		\
	-export-dynamic 						\
	-rpath $(libdir) 						\
	$(no_undefined)							\
	$(LIBTOOL_EXPORT_OPTIONS)

if USE_MEDIALIB
medialib_sources =  \
    gdkmedialib.c
else
medialib_sources =
endif

#
# setup source file variables
#
#
# GDK header files for public installation (non-generated)
#
gdk_public_h_sources =				\
	gdk.h					\
	gdkapplaunchcontext.h			\
	gdkcairo.h				\
	gdkcolor.h				\
	gdkcursor.h				\
	gdkdisplay.h				\
	gdkdisplaymanager.h			\
	gdkdnd.h				\
	gdkdrawable.h				\
	gdkevents.h				\
	gdkfont.h				\
	gdkgc.h					\
	gdki18n.h				\
	gdkimage.h				\
	gdkinput.h				\
	gdkkeys.h				\
	gdkkeysyms.h				\
	gdkkeysyms-compat.h			\
	gdkpango.h				\
	gdkpixbuf.h				\
	gdkpixmap.h				\
	gdkprivate.h				\
	gdkproperty.h				\
	gdkregion.h				\
	gdkrgb.h				\
	gdkscreen.h				\
	gdkselection.h				\
	gdkspawn.h				\
	gdktestutils.h				\
	gdktypes.h				\
	gdkvisual.h				\
	gdkwindow.h

gdk_built_public_sources =			\
	gdkenumtypes.h

gdk_built_private_headers =			\
	gdkalias.h

gdk_private_headers =   \
	gdkinternals.h \
	gdkintl.h \
    gdkpoly-generic.h	\
	gdkregion-generic.h

gdk_c_sources =                 \
	$(medialib_sources)     \
	gdk.c			\
	gdkapplaunchcontext.c	\
	gdkcairo.c		\
	gdkcolor.c		\
	gdkcursor.c		\
	gdkdisplay.c		\
	gdkdisplaymanager.c	\
	gdkdnd.c		\
	gdkdraw.c		\
	gdkevents.c     	\
	gdkfont.c		\
	gdkgc.c			\
	gdkglobals.c		\
	gdkimage.c		\
	gdkkeys.c		\
	gdkkeyuni.c		\
	gdkoffscreenwindow.c	\
	gdkpango.c		\
	gdkpixbuf-drawable.c	\
	gdkpixbuf-render.c	\
	gdkpixmap.c		\
	gdkpolyreg-generic.c	\
	gdkrectangle.c		\
	gdkregion-generic.c	\
	gdkrgb.c		\
	gdkscreen.c		\
	gdkselection.c		\
	gdkvisual.c		\
	gdkwindow.c		\
	gdkwindowimpl.c

gdk_built_sources =				\
	gdkaliasdef.c				\
	gdkenumtypes.c				\
	gdkmarshalers.h 			\
	gdkmarshalers.c				\
	$(gdk_built_public_sources)		\
	$(gdk_built_private_headers)

#
# setup GDK sources and their dependencies
#

gdkincludedir = $(includedir)/gtk-2.0/gdk
gdkinclude_HEADERS = $(gdk_public_h_sources) $(gdk_built_public_sources)

common_sources =                \
	$(gdk_private_headers)      \
    $(gdk_c_sources)	\
	gdkenumtypes.c		\
	gdkmarshalers.c		\
	gdkmarshalers.h

libgdk_directfb_2_0_la_SOURCES = $(common_sources)
libgdk_directfb_2_0_la_LIBADD = directfb/libgdk-directfb.la $(GDK_DEP_LIBS)
libgdk_directfb_2_0_la_LDFLAGS = $(LDADD)

libgdk_x11_2_0_la_SOURCES = $(common_sources)
libgdk_x11_2_0_la_LIBADD = x11/libgdk-x11.la $(GDK_DEP_LIBS)
libgdk_x11_2_0_la_LDFLAGS = $(LDADD)

libgdk_quartz_2_0_la_SOURCES = $(common_sources) gdkkeynames.c
libgdk_quartz_2_0_la_LIBADD = quartz/libgdk-quartz.la $(GDK_DEP_LIBS)
libgdk_quartz_2_0_la_LDFLAGS = $(LDADD)

libgdk_win32_2_0_la_SOURCES = $(common_sources) gdkkeynames.c
libgdk_win32_2_0_la_LIBADD = win32/libgdk-win32.la $(GDK_DEP_LIBS)
libgdk_win32_2_0_la_DEPENDENCIES = win32/libgdk-win32.la win32/rc/gdk-win32-res.o gdk.def
libgdk_win32_2_0_la_LDFLAGS = -Wl,win32/rc/gdk-win32-res.o -export-symbols $(srcdir)/gdk.def $(LDADD)

if HAVE_INTROSPECTION

introspection_files = 		\
	$(filter-out gdkkeysyms-compat.h, $(gdk_public_h_sources))	\
	$(gdk_c_sources)	\
	gdkkeynames.c		\
        gdkenumtypes.c		\
        gdkenumtypes.h

Gdk-2.0.gir: $(gdktargetlib) Makefile
Gdk_2_0_gir_SCANNERFLAGS = --warn-all
Gdk_2_0_gir_INCLUDES = Gio-2.0 GdkPixbuf-2.0 Pango-1.0 cairo-1.0
Gdk_2_0_gir_LIBS = $(gdktargetlib)
Gdk_2_0_gir_FILES = $(introspection_files)
Gdk_2_0_gir_CFLAGS = $(INCLUDES)
INTROSPECTION_GIRS += Gdk-2.0.gir

if USE_X11
x11_introspection_files = 		\
	x11/checksettings.c		\
	x11/gdkapplaunchcontext-x11.c	\
	x11/gdkasync.c			\
	x11/gdkcolor-x11.c		\
	x11/gdkcursor-x11.c		\
	x11/gdkdisplay-x11.c		\
	x11/gdkdnd-x11.c		\
	x11/gdkdrawable-x11.c		\
	x11/gdkgc-x11.c			\
	x11/gdkgeometry-x11.c		\
	x11/gdkglobals-x11.c		\
	x11/gdkimage-x11.c		\
	x11/gdkim-x11.c			\
	x11/gdkinput.c			\
	x11/gdkkeys-x11.c		\
	x11/gdkmain-x11.c		\
	x11/gdkpixmap-x11.c		\
	x11/gdkproperty-x11.c		\
	x11/gdkscreen-x11.c		\
	x11/gdkselection-x11.c		\
	x11/gdksettings.c		\
	x11/gdkspawn-x11.c		\
	x11/gdktestutils-x11.c		\
	x11/gdkvisual-x11.c		\
	x11/gdkwindow-x11.c		\
	x11/gdkxftdefaults.c		\
	x11/gdkxid.c			\
	x11/xsettings-client.c		\
	x11/xsettings-common.c		\
        x11/gdkx.h

GdkX11-2.0.gir: $(gdktargetlib) Gdk-2.0.gir Makefile
GdkX11_2_0_gir_SCANNERFLAGS = --warn-all --strip-prefix=Gdk
GdkX11_2_0_gir_INCLUDES = Gio-2.0 Gdk-2.0 GdkPixbuf-2.0 Pango-1.0 xlib-2.0
GdkX11_2_0_gir_LIBS = $(gdktargetlib)
GdkX11_2_0_gir_FILES = $(x11_introspection_files)
GdkX11_2_0_gir_CFLAGS = $(INCLUDES) -L$(top_builddir)/gdk
INTROSPECTION_GIRS += GdkX11-2.0.gir

introspection_files += $(filter-out x11/gdkx.h, $(x11_introspection_files))

endif # USE_X11

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibsdir = $(libdir)/girepository-1.0
typelibs_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelibs_DATA)

endif # HAVE_INTROSPECTION

if OS_WIN32
install-def-file: gdk.def
	$(INSTALL) $(srcdir)/gdk.def $(DESTDIR)$(libdir)/gdk-win32-2.0.def
uninstall-def-file:
	-rm $(DESTDIR)$(libdir)/gdk-win32-2.0.def
else
install-def-file:
uninstall-def-file:
endif

if MS_LIB_AVAILABLE
noinst_DATA = gdk-win32-$(GTK_API_VERSION).lib

gdk-win32-$(GTK_API_VERSION).lib: libgdk-win32-$(GTK_API_VERSION).la gdk.def
	lib -machine:@LIB_EXE_MACHINE_FLAG@ -name:libgdk-win32-$(GTK_API_VERSION)-@LT_CURRENT_MINUS_AGE@.dll -def:gdk.def -out:$@

install-ms-lib:
	$(INSTALL) gdk-win32-$(GTK_API_VERSION).lib $(DESTDIR)$(libdir)

uninstall-ms-lib:
	-rm $(DESTDIR)$(libdir)/gdk-win32-$(GTK_API_VERSION).lib
else
install-ms-lib:
uninstall-ms-lib:
endif

# This places the generated .def file in srcdir, since it is expected to be there.
# (The one from a tarball is)
gdk.def: gdk.symbols
	(echo -e EXPORTS; $(CPP) -P -DALL_FILES -DGDK_WINDOWING_WIN32 -DINCLUDE_VARIABLES - <$(srcdir)/gdk.symbols | sed -e '/^$$/d' -e 's/^/	/' -e 's/G_GNUC_[^ ]*//g') > $(srcdir)/gdk.def

gdkalias.h: gdk.symbols
	 $(PERL) $(srcdir)/makegdkalias.pl < $(srcdir)/gdk.symbols > gdkalias.h

gdkaliasdef.c: gdk.symbols
	 $(PERL) $(srcdir)/makegdkalias.pl -def < $(srcdir)/gdk.symbols > gdkaliasdef.c

if OS_LINUX
TESTS = abicheck.sh pltcheck.sh
endif


lib_LTLIBRARIES = $(gdktargetlib)

EXTRA_LTLIBRARIES = libgdk-x11-2.0.la libgdk-win32-2.0.la libgdk-quartz-2.0.la libgdk-directfb-2.0.la

MAINTAINERCLEANFILES = $(gdk_built_sources) stamp-gdkenumtypes.h
EXTRA_DIST += $(gdk_built_sources)
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
BUILT_SOURCES = \
	$(gdk_built_sources)			\
	gdkconfig.h

gdkenumtypes.h: stamp-gdkenumtypes.h
	@true
stamp-gdkenumtypes.h: @REBUILD@ $(gdk_public_h_sources) gdkenumtypes.h.template
	( cd $(srcdir) && $(GLIB_MKENUMS) --template gdkenumtypes.h.template \
		$(gdk_public_h_sources) ) >> xgen-geth \
	&& (cmp -s xgen-geth gdkenumtypes.h || cp xgen-geth gdkenumtypes.h ) \
	&& rm -f xgen-geth \
	&& echo timestamp > $(@F)
gdkenumtypes.c: @REBUILD@ $(gdk_public_h_sources) gdkenumtypes.c.template
	( cd $(srcdir) && $(GLIB_MKENUMS) --template gdkenumtypes.c.template \
		$(gdk_public_h_sources) ) > xgen-getc \
	&& cp xgen-getc gdkenumtypes.c  \
	&& rm -f xgen-getc

# 
# Marshaller generation
#
gdkmarshalers.h: @REBUILD@ gdkmarshalers.list
	$(GLIB_GENMARSHAL) --prefix=_gdk_marshal $(srcdir)/gdkmarshalers.list --header > gdkmarshalers-h.tmp \
	&& mv gdkmarshalers-h.tmp gdkmarshalers.h \
	|| ( rm -f gdkmarshalers-h.tmp && exit 1)
gdkmarshalers.c: @REBUILD@ gdkmarshalers.list
	(echo "#include \"gdkalias.h\""; $(GLIB_GENMARSHAL) --prefix=_gdk_marshal $(srcdir)/gdkmarshalers.list --body) > gdkmarshalers-c.tmp	\
	&& mv gdkmarshalers-c.tmp gdkmarshalers.c \
	|| ( rm -f gdkmarshalers-c.tmp && exit 1 )

gdkconfig.h: stamp-gc-h
	@if test -f gdkconfig.h; then :; \
	else rm -f stamp-gc-h; $(MAKE) stamp-gc-h; fi
stamp-gc-h: $(top_builddir)/config.status
	cd $(top_builddir) && $(SHELL) ./config.status gdk/gdkconfig.h
	echo timestamp > stamp-gc-h

dist-hook: ../build/win32/vs9/gdk.vcproj ../build/win32/vs10/gdk.vcxproj ../build/win32/vs10/gdk.vcxproj.filters

../build/win32/vs9/gdk.vcproj: ../build/win32/vs9/gdk.vcprojin
	for F in $(libgdk_win32_2_0_la_SOURCES); do \
		case $$F in \
		*.c) echo '   <File RelativePath="..\..\..\gdk\'$$F'" />' \
		     ;; \
		esac; \
	done >libgdk.sourcefiles
	$(CPP) -P - <$(top_srcdir)/build/win32/vs9/gdk.vcprojin >$@
	rm libgdk.sourcefiles
	
../build/win32/vs10/gdk.vcxproj: ../build/win32/vs10/gdk.vcxprojin
	for F in $(libgdk_win32_2_0_la_SOURCES); do \
		case $$F in \
		*.c) echo '   <ClCompile Include="..\..\..\gdk\'$$F'" />' \
		     ;; \
		esac; \
	done >libgdk.vs10.sourcefiles
	$(CPP) -P - <$(top_srcdir)/build/win32/vs10/gdk.vcxprojin >$@
	rm libgdk.vs10.sourcefiles
	
../build/win32/vs10/gdk.vcxproj.filters: ../build/win32/vs10/gdk.vcxproj.filtersin
	for F in $(libgdk_win32_2_0_la_SOURCES); do \
		case $$F in \
		*.c) echo '   <ClCompile Include="..\..\..\gdk\'$$F'"><Filter>Source Files</Filter></ClCompile>' \
		     ;; \
		esac; \
	done >libgdk.vs10.sourcefiles.filters
	$(CPP) -P - <$(top_srcdir)/build/win32/vs10/gdk.vcxproj.filtersin >$@
	rm libgdk.vs10.sourcefiles.filters

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
