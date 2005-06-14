## Process this file with automake to produce Makefile.in

libgdkincludedir = $(includedir)/gtk-2.0/gdk

AUTOMAKE_OPTIONS = no-dependencies

INCLUDES = 				\
	-DG_LOG_DOMAIN=\"Gdk\"		\
	-DGDK_COMPILATION		\
	-I$(top_srcdir)			\
	-I$(top_srcdir)/gdk		\
	-I$(top_builddir)/gdk		\
	-DG_DISABLE_DEPRECATED		\
	-DGDK_PIXBUF_DISABLE_DEPRECATED	\
	-DGDK_DISABLE_DEPRECATED	\
	$(GTK_DEBUG_FLAGS) 		\
	$(GDK_DEP_CFLAGS)

LDADDS = $(GDK_DEP_LIBS)

noinst_LTLIBRARIES = libgdk-macosx.la

libgdk_macosx_la_SOURCES = gdkpango-macosx.c \
	gdkevents-macosx.m \
	gdkdisplay-macosx.m \
	gdkgc-macosx.m \
	gdkmain-macosx.m \
	gdknsview-macosx.m \
	gdkscreen-macosx.m \
	gdkwindow-macosx.m


libgdkinclude_HEADERS = 

# We need to include all these C files here since the conditionals
# don't seem to be correctly expanded for the dist files.
EXTRA_DIST =
