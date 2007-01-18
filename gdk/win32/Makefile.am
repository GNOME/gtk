## Process this file with automake to produce Makefile.in

libgdkincludedir = $(includedir)/gtk-2.0/gdk

INCLUDES = \
	-DG_LOG_DOMAIN=\"Gdk\"	\
	-DINSIDE_GDK_WIN32	\
	-I$(top_srcdir)		\
	-I$(top_srcdir)/gdk	\
	-I$(top_builddir)/gdk	\
	$(GTK_DEBUG_FLAGS) 	\
	$(GDK_DEP_CFLAGS)	\
	$(GDK_WIN32_EXTRA_CFLAGS)\
	-DGDK_COMPILATION

LDADDS = $(GDK_DEP_LIBS)

if HAVE_WINTAB
libwntab32x_a = libwntab32x.a

libwntab32x.a: $(WINTAB_LIB)
	cp $< $@
endif

if HAVE_IE55
libie55uuid_a = libie55uuid.a

libie55uuid.a: $(IE55_UUID_LIB)
	cp $< $@
endif

noinst_DATA = $(libwntab32x_a) $(libie55uuid_a)

noinst_LTLIBRARIES = libgdk-win32.la

SUBDIRS=rc

EXTRA_DIST = \
	bdfcursor.c \
	makefile.msc

libgdk_win32_la_SOURCES = \
	xcursors.h \
	gdkcolor-win32.c \
	gdkcursor-win32.c \
	gdkdisplay-win32.c \
	gdkdnd-win32.c \
	gdkdrawable-win32.c \
	gdkdrawable-win32.h \
	gdkevents-win32.c \
	gdkfont-win32.c \
	gdkgc-win32.c \
	gdkgeometry-win32.c \
	gdkglobals-win32.c \
	gdkim-win32.c \
	gdkimage-win32.c \
	gdkinput.c \
	gdkinput-win32.c \
	gdkinput-win32.h \
	gdkkeys-win32.c \
	gdkmain-win32.c \
	gdkpixmap-win32.c \
	gdkpixmap-win32.h \
	gdkprivate-win32.h \
	gdkproperty-win32.c \
	gdkscreen-win32.c \
	gdkselection-win32.c \
	gdkspawn-win32.c \
	gdkvisual-win32.c \
	gdkwin32.h \
	gdkwin32id.c \
	gdkwindow-win32.c \
	gdkwindow-win32.h

libgdkinclude_HEADERS =		\
	gdkwin32.h
