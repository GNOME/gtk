## Process this file with automake to produce Makefile.in

gdkincludedir = $(includedir)/gdk



lib_LTLIBRARIES = libgdk.la

INCLUDES = \
	-DG_LOG_DOMAIN=\"Gdk\"	\
	-I$(top_srcdir)		\
	@GTK_DEBUG_FLAGS@ 	\
	@GTK_XIM_FLAGS@		\
	@GTK_LOCALE_FLAGS@	\
	@GLIB_CFLAGS@		\
	@x_cflags@ 

libgdk_la_SOURCES = \
	gdk.c		\
	gdkcc.c		\
	gdkcolor.c	\
	gdkcursor.c	\
	gdkdnd.c	\
	gdkdraw.c	\
	gdkevents.c     \
	gdkfont.c	\
	gdkgc.c		\
	gdkglobals.c	\
	gdkim.c		\
	gdkimage.c	\
	gdkinput.c	\
	gdkinput.h	\
	gdkinputnone.h  \
	gdkinputcommon.h\
	gdkinputgxi.h	\
	gdkinputxfree.h	\
	gdkpixmap.c	\
	gdkproperty.c	\
	gdkrgb.c	\
	gdkrectangle.c	\
	gdkregion.c	\
	gdkselection.c	\
	gdkvisual.c	\
	gdkwindow.c	\
	gdkxid.c 	\
	MwmUtil.h       \
	gxid_lib.h	\
	gxid_proto.h	\
        gxid_lib.c
## this last one is ifdef'd out unless XINPUT_GXI is defined
## It's easier than trying to get automake to handle compiling
## it conditionally

gdkinclude_HEADERS = \
	gdk.h		\
	gdkcursors.h	\
	gdkrgb.h	\
	gdki18n.h	\
	gdkkeysyms.h	\
	gdkprivate.h	\
	gdktypes.h	\
	gdkx.h

LDADDS = \
	@x_ldflags@	\
	@x_libs@	\
	@GLIB_LIBS@	\
	-lm

libgdk_la_LDFLAGS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) -release $(LT_RELEASE) \
         @GLIB_DEPLIBS@	\
         @x_ldflags@	\
         @x_libs@	\
         -lm

configexecincludedir = $(pkglibdir)/include
#configexecinclude_DATA = gdkconfig.h
install-exec-local: gdkconfig.h
	$(mkinstalldirs) $(DESTDIR)$(configexecincludedir)
	file=$(DESTDIR)$(configexecincludedir)/gdkconfig.h; \
	if test -r $$file && cmp -s gdkconfig.h $$file; then :; \
	else $(INSTALL_DATA) gdkconfig.h $$file; fi

BUILT_SOURCES = stamp-gc-h #note: not gdkconfig.h
gdkconfig.h: stamp-gc-h
	@if test -f gdkconfig.h; then :; \
	else rm -f stamp-gc-h; $(MAKE) stamp-gc-h; fi
stamp-gc-h: ../config.status
	cd .. && CONFIG_FILES= CONFIG_HEADERS= CONFIG_OTHER=gdk/gdkconfig.h ./config.status
	echo timestamp > stamp-gc-h

EXTRA_PROGRAMS = gxid

bin_PROGRAMS = @xinput_progs@

gxid_SOURCES = gxid.c

gxid_LDADD = $(LDADDS)

X-derived-headers:
	sed -e 's/^#define[ 	]*XC\([^ 	]*\)[ 	]*\([^ 	]*\)[ 	]*.*$$/GDK\1 = \2,/' \
	    -e 'tb' -e 'd' -e ':b' \
	    -e 'y/abcdefghijklmnopqrstuvwxyz/ABCDEFGHIJKLMNOPQRSTUVWXYZ/' \
	    < @x_includes@/X11/cursorfont.h > gdkcursors.h ; \
	sed -e 's/^#define[ 	]*XK\([^ 	]*\)[ 	]*\([^ 	]*\)[ 	]*.*$$/#define GDK\1 \2/' \
	    -e 'tb' -e 'd' -e ':b' -e 's/ 0X/ 0x/' \
	    < @x_includes@/X11/keysymdef.h > gdkkeysyms.h

.PHONY: files

files:
	@files=`ls $(DISTFILES) 2> /dev/null `; for p in $$files; do \
	  echo $$p; \
	done
