## Makefile.am for gtk+/gdk

SUBDIRS=$(gdktarget)
DIST_SUBDIRS=linux-fb win32 x11

EXTRA_DIST =			\
	gdkconfig.h.win32 	\
	gdk.def 		\
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
	$(INSTALL) .libs/libgdk-win32-1.3.dll.a $(DESTDIR)$(libdir)
uninstall-libtool-import-lib:
	-rm $(DESTDIR)$(libdir)/libdk-win32-1.3.dll.a
else
install-libtool-import-lib:
uninstall-libtool-import-lib:
endif

if MS_LIB_AVAILABLE
noinst_DATA = gdk-win32-1.3.lib

gdk-win32-1.3.lib: libgdk-win32-1.3.la gdk.def
	lib -name:libgdk-win32-1.3-@LT_CURRENT_MINUS_AGE@.dll -def:gdk.def -out:$@

install-ms-lib:
	$(INSTALL) gdk-win32-1.3.lib $(DESTDIR)$(libdir)

uninstall-ms-lib:
	-rm $(DESTDIR)$(libdir)/gdk-win32-1.3.lib
else
install-ms-lib:
uninstall-ms-lib:
endif

# libtool stuff: set version and export symbols for resolving
# since automake doesn't support conditionalized libsomething_la_LDFLAGS
# we use the general approach here
LDFLAGS = @STRIP_BEGIN@ 					\
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) 	\
	-export-dynamic 					\
	-rpath @prefix@/lib 					\
	$(no_undefined)						\
	@LIBTOOL_EXPORT_OPTIONS@				\
	$(top_builddir)/gdk-pixbuf/libgdk_pixbuf-1.3.la		\
	$(gdk_win32_symbols)					\
	@GDK_DEP_LIBS@						\
@STRIP_END@

#
# setup source file variables
#
#
# GDK header files for public installation (non-generated)
#
# Note: files added here may need to be be propagated to gdk_headers in gtk/Makefile.am
#
gdk_public_h_sources = @STRIP_BEGIN@ \
	gdk.h		\
	gdkcolor.h	\
	gdkcursor.h     \
	gdkcursors.h	\
	gdkdnd.h	\
	gdkdrawable.h	\
	gdkevents.h	\
	gdkfont.h	\
	gdkgc.h		\
	gdkkeysyms.h	\
	gdki18n.h	\
	gdkimage.h	\
	gdkinput.h	\
	gdkkeys.h	\
	gdkpango.h	\
	gdkpixbuf.h	\
	gdkpixmap.h	\
	gdkproperty.h	\
	gdkregion.h	\
	gdkrgb.h	\
	gdkselection.h	\
	gdktypes.h	\
	gdkvisual.h	\
	gdkwindow.h	\
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
	gdkwindow.c		\
@STRIP_END@

#
# setup GDK sources and their dependancies
#
extra = @gdktargetlib@

if USE_X11
libgdk_x11_includedir = $(includedir)/gtk-2.0/gdk

libgdk_x11_1_3_la_LIBADD = \
	$(gtarget)/libgdk-$(gtarget).la	

libgdk_x11_include_HEADERS = $(gdk_headers)
libgdk_x11_1_3_la_SOURCES = $(gdk_c_sources) gdkenumtypes.c
endif

if USE_LINUX_FB
libgdk_linux_fb_includedir = $(includedir)/gtk-2.0/gdk

libgdk_linux_fb_1_3_la_LIBADD = \
	$(gtarget)/libgdk-$(gtarget).la	

libgdk_linux_fb_include_HEADERS = $(gdk_headers)
libgdk_linux_fb_1_3_la_SOURCES = $(gdk_c_sources) gdkenumtypes.c
endif

if USE_WIN32
libgdk_win32_includedir = $(includedir)/gtk-2.0/gdk

libgdk_win32_1_3_la_LIBADD = \
	$(gtarget)/libgdk-$(gtarget).la $(wintab_lib) $(ie55uuid_lib)

libgdk_win32_include_HEADERS = $(gdk_headers)
libgdk_win32_1_3_la_SOURCES = $(gdk_c_sources) gdkenumtypes.c

endif

# now define the real one to workaround automake's mishandling
lib_LTLIBRARIES = $(extra)

EXTRA_LTLIBRARIES = libgdk-x11-1.3.la libgdk-linux-fb-1.3.la libgdk-win32-1.3.la

MAINTAINERCLEANFILES =
EXTRA_HEADERS =

#
# rules to generate built sources
#
# we only need to remake these headers once a new X version is released
X-derived-headers:
	sed -e 's/^#define[ 	]*XC\([^ 	]*\)[ 	]*\([^ 	]*\)[ 	]*.*$$/GDK\1 = \2,/' \
	    -e 'tb' -e 'd' -e ':b' \
	    -e 'y/abcdefghijklmnopqrstuvwxyz/ABCDEFGHIJKLMNOPQRSTUVWXYZ/' \
	    < @x_includes@/X11/cursorfont.h > gdkcursors.h ; \
	sed -e 's/^#define[ 	]*XK\([^ 	]*\)[ 	]*\([^ 	]*\)[ 	]*.*$$/#define GDK\1 \2/' \
	    -e 'tb' -e 'd' -e ':b' -e 's/ 0X/ 0x/' \
	    < @x_includes@/X11/keysymdef.h > gdkkeysyms.h

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

#note: not gdkconfig.h
BUILT_SOURCES = stamp-gc-h @REBUILD@ gdkenumtypes.c gdkenumtypes.h

gdkenumtypes.h: @REBUILD@ $(gdk_public_h_sources) $(srcdir)/makeenums.pl
	cd $(srcdir) \
	&& $(PERL) ./makeenums.pl include $(gdk_public_h_sources) > gdkenumtypes.h

gdkenumtypes.c: @REBUILD@ $(gdk_public_h_sources) $(srcdir)/makeenums.pl
	cd $(srcdir) \
	&& $(PERL) ./makeenums.pl cfile $(gdk_public_h_sources) > gdkenumtypes.c

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
