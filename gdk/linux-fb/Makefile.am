## Process this file with automake to produce Makefile.in

INCLUDES = @STRIP_BEGIN@ 	\
	-DG_LOG_DOMAIN=\"Gdk\"	\
	-I$(top_srcdir)		\
	-I$(top_srcdir)/gdk	\
	-I$(top_builddir)/gdk	\
	@GTK_DEBUG_FLAGS@ 	\
	@GTK_XIM_FLAGS@		\
	@GTK_LOCALE_FLAGS@	\
	@GLIB_CFLAGS@		\
@STRIP_END@

LDFLAGS = @STRIP_BEGIN@ \
	@GLIB_LIBS@	\
	-L/gnome2/lib   \
	-lt1		\
	-lm		\
@STRIP_END@

if USE_LINUX_FB

lib_LTLIBRARIES = libgdk-fb.la
noinst_PROGRAMS=test-fb

test_fb_LDFLAGS=../libgdk.la libgdk-fb.la

libgdk_fb_la_SOURCES =    	\
	gdkcolor-fb.c	   	\
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
	gdkevents-fb.c		\
	miarc.c			\
	midash.c		\
	mifillarc.c		\
	mifpolycon.c		\
	mipoly.c		\
	mipolygen.c		\
	mipolyutil.c		\
	miwideline.c		\
	mizerclip.c		\
	mizerline.c		\
	mispans.c
else
lib_LTLIBRARIES =
noinst_PROGRAMS=

endif

