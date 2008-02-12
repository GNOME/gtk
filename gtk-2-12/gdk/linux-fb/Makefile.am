## Process this file with automake to produce Makefile.in

bin_PROGRAMS =

if ENABLE_FB_MANAGER
bin_PROGRAMS += gdkfbmanager gdkfbswitch
endif

libgdkincludedir = $(includedir)/gtk-2.0/gdk
libgdkfbincludedir = $(includedir)/gtk-2.0/gdk/linux-fb

INCLUDES = \
	-DG_LOG_DOMAIN=\"Gdk\"	\
	-DGDK_DATA_PREFIX=\"$(prefix)\"	\
	-I$(top_srcdir)		\
	-I$(top_srcdir)/gdk	\
	-I$(top_builddir)/gdk	\
	-DGDK_PIXBUF_DISABLE_DEPRECATED	\
	$(GDK_DEP_CFLAGS)	\
	$(GTK_DEBUG_FLAGS)

noinst_LTLIBRARIES = libgdk-linux-fb.la

libgdkinclude_HEADERS= \
	gdkfb.h

libgdk_linux_fb_la_SOURCES =    \
	gdkcolor-fb.c	   	\
	gdkcursor-fb.c	   	\
	gdkdisplay-fb.c		\
	gdkdnd-fb.c	   	\
	gdkdrawable-fb2.c  	\
	gdkevents-fb.c		\
	gdkfbmanager.h		\
	gdkfont-fb.c	   	\
	gdkgc-fb.c 	   	\
	gdkgeometry-fb.c  	\
	gdkglobals-fb.c   	\
	gdkim-fb.c	   	\
	gdkimage-fb.c	   	\
	gdkinput.c	   	\
	gdkinputprivate.h	\
	gdkkeyboard-fb.c	\
	gdkmain-fb.c	   	\
	gdkmouse-fb.c		\
	gdkpango-fb.c		\
	gdkpixmap-fb.c	   	\
	gdkprivate-fb.h   	\
	gdkproperty-fb.c  	\
	gdkrender-fb.c		\
	gdkscreen-fb.c		\
	gdkselection-fb.c 	\
	gdkspawn-fb.c		\
	gdkvisual-fb.c	   	\
	gdkwindow-fb.c    	\
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
	mispans.c		\
	mispans.h		\
	mistruct.h		\
	mitypes.h		\
	miwideline.c		\
	miwideline.h		\
	mizerclip.c		\
	mizerline.c

gdkfbmanager_sources = gdkfbmanager.c 
gdkfbmanager_LDFLAGS = $(GLIB_LIBS)
gdkfbswitch_sources = gdkfbswitch.c
gdkfbswitch_LDFLAGS = $(GLIB_LIBS)

EXTRA_DIST=x-cursors.xbm
