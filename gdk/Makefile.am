## Process this file with automake to produce Makefile.in

gdkincludedir = $(includedir)/gdk

lib_LTLIBRARIES = libgdk-1.1.la
DEFS += -DG_LOG_DOMAIN=\"Gdk\"

libgdk_1_1_la_SOURCES = \
	gdk.c		\
	gdkcc.c		\
	gdkcolor.c	\
	gdkcursor.c	\
	gdkdnd.c	\
	gdkdraw.c	\
	gdkfont.c	\
	gdkgc.c		\
	gdkglobals.c	\
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
	gdkthreads.c    \
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

libgdk_1_1_la_LDFLAGS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
	@x_ldflags@ @x_libs@

INCLUDES = -I$(top_srcdir) @GLIB_CFLAGS@ @x_cflags@ 

EXTRA_PROGRAMS = gxid

bin_PROGRAMS = @xinput_progs@

gxid_SOURCES = gxid.c

gxid_LDADD = \
	@x_ldflags@	\
	@x_libs@	\
	-lm

BUILT_SOURCES = gdkcursors.h gdkkeysyms.h

EXTRA_DIST = makecursors.awk makekeysyms.awk

gdkcursors.h:
	awk -f $(srcdir)/makecursors.awk @x_includes@/X11/cursorfont.h > $@

gdkkeysyms.h:
	awk -f $(srcdir)/makekeysyms.awk @x_includes@/X11/keysymdef.h > $@

.PHONY: files

files:
	@files=`ls $(DISTFILES) 2> /dev/null `; for p in $$files; do \
	  echo $$p; \
	done
