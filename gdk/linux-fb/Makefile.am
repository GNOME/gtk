## Process this file with automake to produce Makefile.in

libgdkincludedir = $(includedir)/gtk-2.0/gdk
libgdkfbincludedir = $(includedir)/gtk-2.0/gdk/linux-fb

INCLUDES = @STRIP_BEGIN@ 	\
	-DG_LOG_DOMAIN=\"Gdk\"	\
	-I$(top_srcdir)		\
	-I$(top_srcdir)/gdk	\
	-I$(top_builddir)/gdk	\
	@FREETYPE_CFLAGS@	\
	@GTK_DEBUG_FLAGS@ 	\
	@GTK_XIM_FLAGS@		\
	@GTK_LOCALE_FLAGS@	\
	@GLIB_CFLAGS@		\
	@PANGO_CFLAGS@		\
@STRIP_END@

LDFLAGS = @STRIP_BEGIN@ \
	@GLIB_LIBS@	\
	@PANGO_LIBS@	\
	@FREETYPE_LIBS@	\
	-lm		\
@STRIP_END@

noinst_LTLIBRARIES = libgdk-linux-fb.la

libgdkinclude_HEADERS= \
	gdkfb.h

libgdk_linux_fb_la_SOURCES =    \
	gdkcolor-fb.c	   	\
	gdkcc-fb.c		\
	gdkcursor-fb.c	   	\
	gdkdnd-fb.c	   	\
	gdkdrawable-fb2.c  	\
	gdkfont-fb.c	   	\
	gdkgc-fb.c 	   	\
	gdkgeometry-fb.c  	\
	gdkglobals-fb.c   	\
	gdkim-fb.c	   	\
	gdkimage-fb.c	   	\
	gdkinput.c	   	\
	gdkmain-fb.c	   	\
	gdkpixmap-fb.c	   	\
	gdkproperty-fb.c  	\
	gdkselection-fb.c 	\
	gdkvisual-fb.c	   	\
	gdkwindow-fb.c    	\
	gdkprivate-fb.h   	\
	gdkinputprivate.h	\
	gdkinput-ps2.c		\
	gdkmouse-fb.c		\
	gdkevents-fb.c		\
	gdkrender-fb.c		\
	mi.h			\
	miarc.c			\
	midash.c		\
	mifillarc.c		\
	mifillarc.h		\
	mifpoly.h		\
	mifpolycon.c		\
	miline.h		\
	mipoly.c		\
	mipoly.h		\
	mipolygen.c		\
	mipolyutil.c		\
	miscanfill.h		\
	mispans.h		\
	mistruct.h		\
	mitypes.h		\
	miwideline.c		\
	miwideline.h		\
	mizerclip.c		\
	mizerline.c		\
	mispans.c		\
	gdkpango-fb.c
	mispans.c

EXTRA_DIST=x-cursors.xbm
