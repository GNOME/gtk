## Process this file with automake to produce Makefile.in

INCLUDES = @STRIP_BEGIN@ 	\
	-DG_LOG_DOMAIN=\"Gdk\"	\
	-I$(top_srcdir)		\
	-I$(top_srcdir)/gdk	\
	@GTK_DEBUG_FLAGS@ 	\
	@GTK_XIM_FLAGS@		\
	@GTK_LOCALE_FLAGS@	\
	@GLIB_CFLAGS@		\
	@x_cflags@ 		\
@STRIP_END@

LDADDS = @STRIP_BEGIN@ 	\
	@x_ldflags@	\
	@x_libs@	\
	@GLIB_LIBS@	\
	-lm		\
@STRIP_END@

noinst_LTLIBRARIES = libgdk-nanox.la

xinput_sources =	   \
	gdkinput-none.c

libgdk_nanox_la_SOURCES =    \
	gdkcc-nanox.c	   \
	gdkcolor-nanox.c	   \
	gdkcursor-nanox.c	   \
	gdkdnd-nanox.c	   \
	gdkdrawable-nanox.c  \
	gdkevents-nanox.c	   \
	gdkfont-nanox.c	   \
	gdkgc-nanox.c 	   \
	gdkglobals-nanox.c   \
	gdkim-nanox.c	   \
	gdkimage-nanox.c	   \
	gdkinput.c	   \
	gdkmain-nanox.c	   \
	gdkpixmap-nanox.c	   \
	gdkpolyreg-generic.c	\
	gdkproperty-nanox.c  \
	gdkregion-generic.c	   \
	gdkselection-nanox.c \
	gdkvisual-nanox.c	   \
	gdkwindow-nanox.c    \
	gdknanox.h		   \
	gdkprivate-nanox.h   \
	gdkinput-none.c


