## Process this file with automake to produce Makefile.in

INCLUDES = @STRIP_BEGIN@ 	\
	-DG_LOG_DOMAIN=\"Gdk\"	\
	-I$(top_srcdir)		\
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

noinst_LTLIBRARIES = libgdk-x11.la

libgdk_x11_la_SOURCES =  \
	gxid_lib.c	 \
	gxid_lib.h	 \
	gxid_proto.h

EXTRA_PROGRAMS = gxid
bin_PROGRAMS = @xinput_progs@

gxid_SOURCES = gxid.c
gxid_LDADD = $(LDADDS)

#EXTRA_DIST = \
#	MwmUtil.h         \


