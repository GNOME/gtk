## Process this file with automake to produce Makefile.in

libgdkincludedir = $(includedir)/gtk-2.0/gdk
libgdkx11includedir = $(includedir)/gtk-2.0/gdk/x11

INCLUDES = @STRIP_BEGIN@ 	\
	-DG_LOG_DOMAIN=\"Gdk\"	\
	-I$(top_srcdir)		\
	-I$(top_srcdir)/gdk	\
	-I$(top_builddir)/gdk	\
	@GTK_DEBUG_FLAGS@ 	\
	@GTK_XIM_FLAGS@		\
	@GTK_LOCALE_FLAGS@	\
	@PANGO_CFLAGS@		\
	@GLIB_CFLAGS@		\
	@FREETYPE_CFLAGS@	\
	@x_cflags@ 		\
@STRIP_END@

LDADDS = @STRIP_BEGIN@ 	\
	@x_ldflags@	\
	@x_libs@	\
	@GLIB_LIBS@	\
	-lm		\
@STRIP_END@

noinst_LTLIBRARIES = libgdk-x11.la

if XINPUT_GXI
xinput_sources =	   \
	gdkinput-x11.c	   \
	gdkinput-gxi.c
else
if XINPUT_XFREE
xinput_sources =	   \
	gdkinput-x11.c	   \
	gdkinput-xfree.c
else
xinput_sources =	   \
	gdkinput-none.c
endif
endif

libgdk_x11_la_SOURCES =    	\
	MwmUtil.h	   	\
	gdkcolor-x11.c	   	\
	gdkcursor-x11.c	   	\
	gdkdnd-x11.c	   	\
	gdkdrawable-x11.c  	\
	gdkevents-x11.c	   	\
	gdkfont-x11.c	   	\
	gdkgc-x11.c 	   	\
	gdkgeometry-x11.c  	\
	gdkglobals-x11.c   	\
	gdkim-x11.c	   	\
	gdkimage-x11.c	   	\
	gdkinput.c	   	\
	gdkkeys-x11.c		\
	gdkmain-x11.c	   	\
	gdkpango-x11.c		\
	gdkpixmap-x11.c	   	\
	gdkproperty-x11.c  	\
	gdkselection-x11.c 	\
	gdkvisual-x11.c	   	\
	gdkwindow-x11.c    	\
	gdkxid.c	   	\
	gxid_lib.c	   	\
	gxid_lib.h	   	\
	gxid_proto.h	   	\
	gdkx.h		   	\
	gdkprivate-x11.h   	\
	gdkinputprivate.h  	\
	xsettings-client.h	\
	xsettings-client.c	\
	xsettings-common.h	\
	xsettings-common.c	\
	$(xinput_sources)

libgdkinclude_HEADERS = 	\
	gdkx.h

libgdkx11include_HEADERS = 	\
	gdkdrawable-x11.h	\
	gdkwindow-x11.h		\
	gdkpixmap-x11.h

EXTRA_PROGRAMS = gxid
bin_PROGRAMS = @xinput_progs@

gxid_SOURCES = gxid.c
gxid_LDADD = $(LDADDS)

# We need to include all these C files here since the conditionals
# don't seem to be correctly expanded for the dist files.
EXTRA_DIST = 			\
	gdkinput-x11.c		\
	gdkinput-xfree.c	\
	gdkinput-gxi.c		\
	gdkinput-none.c
