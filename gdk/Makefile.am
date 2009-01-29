## Makefile.am for gtk+/gdk
include $(top_srcdir)/Makefile.decl

SUBDIRS = $(gdktarget) . tests
DIST_SUBDIRS = win32 x11 quartz directfb tests

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
	-I$(top_builddir)/gdk		\
	-I$(top_srcdir)/gdk-pixbuf 	\
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
	gdkinternals.h  	\
	gdkintl.h		\
	gdkkeys.c		\
	gdkkeyuni.c		\
	gdkpango.c		\
	gdkpixbuf-drawable.c	\
	gdkpixbuf-render.c	\
	gdkpixmap.c		\
	gdkpoly-generic.h	\
	gdkpolyreg-generic.c	\
	gdkrectangle.c		\
	gdkregion-generic.c	\
	gdkregion-generic.h	\
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

# gdkmarshalers.c is not here because it is currently an empty file
common_sources =                \
	$(gdk_c_sources)	\
	gdkenumtypes.c		\
	gdkmarshalers.h

libgdk_directfb_2_0_la_SOURCES = $(common_sources) 
libgdk_directfb_2_0_la_LIBADD = directfb/libgdk-directfb.la $(GDK_DEP_LIBS) \
  $(top_builddir)/gdk-pixbuf/libgdk_pixbuf-$(GTK_API_VERSION).la
libgdk_directfb_2_0_la_LDFLAGS = $(LDADD)

libgdk_x11_2_0_la_SOURCES = $(common_sources)
libgdk_x11_2_0_la_LIBADD = x11/libgdk-x11.la $(GDK_DEP_LIBS) \
  $(top_builddir)/gdk-pixbuf/libgdk_pixbuf-$(GTK_API_VERSION).la
libgdk_x11_2_0_la_LDFLAGS = $(LDADD)

libgdk_quartz_2_0_la_SOURCES = $(common_sources) gdkkeynames.c
libgdk_quartz_2_0_la_LIBADD = quartz/libgdk-quartz.la $(GDK_DEP_LIBS) \
  $(top_builddir)/gdk-pixbuf/libgdk_pixbuf-$(GTK_API_VERSION).la
libgdk_quartz_2_0_la_LDFLAGS = $(LDADD)

libgdk_win32_2_0_la_SOURCES = $(common_sources) gdkkeynames.c
libgdk_win32_2_0_la_LIBADD = win32/libgdk-win32.la $(GDK_DEP_LIBS) \
  $(top_builddir)/gdk-pixbuf/libgdk_pixbuf-$(GTK_API_VERSION).la
libgdk_win32_2_0_la_DEPENDENCIES = win32/libgdk-win32.la win32/rc/gdk-win32-res.o gdk.def
libgdk_win32_2_0_la_LDFLAGS = -Wl,win32/rc/gdk-win32-res.o -export-symbols $(srcdir)/gdk.def $(LDADD)

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
