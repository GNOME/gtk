## Process this file with automake to produce Makefile.in

INCLUDES = @STRIP_BEGIN@ 	\
	-DG_LOG_DOMAIN=\"Gdk\"	\
	-I$(top_srcdir)		\
	-I$(top_srcdir)/gdk	\
	-I$(top_builddir)/gdk	\
	-I$(top_srcdir)/gdk/linux-fb \
	@FREETYPE_CFLAGS@	\
	@GTK_DEBUG_FLAGS@ 	\
	@GTK_XIM_FLAGS@		\
	@GTK_LOCALE_FLAGS@	\
	@GLIB_CFLAGS@		\
	@PANGO_CFLAGS@		\
@STRIP_END@

LDFLAGS = @STRIP_BEGIN@ \
	@GLIB_LIBS@	\
	@PANGO_CFLAGS@	\
	@FREETYPE_LIBS@ \
	-lm		\
@STRIP_END@

fb_basic_la_SOURCES = basic.c

fb_basic_la_LDFLAGS = -export-dynamic -avoid-version -module
fb_basic_la_LIBADD = $(PANGO_LIBS) \
	-L$(top_builddir)/gdk/.libs -lgdk-linux-fb-1.3 \
	-L$(top_builddir)/gdk-pixbuf/.libs -lgdk_pixbuf-1.3

if USE_LINUX_FB

moduledir = $(libdir)/pango/modules
module_LTLIBRARIES = fb-basic.la

endif

EXTRA_DIST =            \
        tables-big.i    \
        tables-small.i
