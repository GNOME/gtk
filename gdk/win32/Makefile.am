## Process this file with automake to produce Makefile.in
include $(top_srcdir)/Makefile.decl

libgdkincludedir = $(includedir)/gtk-3.0/gdk
libgdkwin32includedir = $(includedir)/gtk-3.0/gdk/win32

AM_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"Gdk\"		\
	-DG_LOG_USE_STRUCTURED=1	\
	-DINSIDE_GDK_WIN32		\
	-I$(top_srcdir)			\
	-I$(top_srcdir)/gdk		\
	-I$(top_builddir)/gdk		\
	$(GDK_HIDDEN_VISIBILITY_CFLAGS) \
	$(GTK_DEBUG_FLAGS) 		\
	$(GDK_DEP_CFLAGS)		\
	$(GDK_WIN32_EXTRA_CFLAGS)	\
	-DGDK_COMPILATION

if WIN32_GLES
AM_CPPFLAGS += "-DGDK_WIN32_ENABLE_EGL=1"
endif #WIN32_GLES

libgdk_win32_DEP_LIBS = \
	-lhid

LDADDS = $(libgdk_win32_DEP_LIBS) $(GDK_DEP_LIBS)

noinst_LTLIBRARIES = libgdk-win32.la

SUBDIRS=rc

EXTRA_DIST += \
	bdfcursor.c \
	makefile.msc \
	meson.build

libgdk_win32_la_SOURCES = \
	gdkcursor-win32.c \
	gdkdevicemanager-win32.c \
	gdkdevicemanager-win32.h \
	gdkdevice-virtual.c \
	gdkdevice-virtual.h \
	gdkdevice-win32.c \
	gdkdevice-win32.h \
	gdkdevice-winpointer.c \
	gdkdevice-winpointer.h \
	gdkdevice-wintab.c \
	gdkdevice-wintab.h \
	gdkdisplay-win32.c \
	gdkdisplay-win32.h \
	gdkdisplaymanager-win32.c \
	gdkdnd-win32.c \
	gdkevents-win32.c \
	gdkgeometry-win32.c \
	gdkglcontext-win32.c \
	gdkglcontext-win32.h \
	gdkglobals-win32.c \
	gdkkeys-win32.c \
	gdkkeys-win32-impl.c \
	gdkkeys-win32-impl-wow64.c \
	gdkmain-win32.c \
	gdkmonitor-win32.c \
	gdkmonitor-win32.h \
	gdkprivate-win32.h \
	gdkproperty-win32.c \
	gdkscreen-win32.c \
	gdkselection-win32.c \
	gdkselection-win32.h \
	gdktestutils-win32.c \
	gdkwin32cursor.h \
	gdkwin32display.h \
	gdkwin32displaymanager.h \
	gdkwin32dnd.h \
	gdkwin32dnd-private.h \
	gdkwin32glcontext.h \
	gdkwin32langnotification.c \
	gdkwin32langnotification.h \
	gdkwin32.h \
	gdkwin32id.c \
	gdkwin32keys.h \
	gdkwin32monitor.h \
	gdkwin32screen.h \
	gdkwin32window.h \
	gdkwindow-win32.c \
	gdkwindow-win32.h \
	pktdef.h \
	winpointer.h \
	wintab.h \
	xcursors.h

libgdk_win32_la_LIBADD = \
	$(LDADDS) \
	$(NULL)

libgdkinclude_HEADERS =		\
	gdkwin32.h

libgdkwin32include_HEADERS =	\
	gdkwin32cursor.h	\
	gdkwin32display.h	\
	gdkwin32displaymanager.h\
	gdkwin32dnd.h		\
	gdkwin32glcontext.h	\
	gdkwin32keys.h		\
	gdkwin32misc.h		\
	gdkwin32monitor.h	\
	gdkwin32screen.h	\
	gdkwin32window.h

# ------------------- MSVC Build Items ----------------
MSVCPROJS = gdk3-win32

gdk3_win32_FILES = $(libgdk_win32_la_SOURCES)
gdk3_win32_EXCLUDES = dummy
gdk3_win32_HEADERS_DIR = $(libgdkwin32includedir)

gdk3_win32_HEADERS_INST = $(libgdkwin32include_HEADERS)

gdk3_win32_HEADERS_EXCLUDES = dummy

include $(top_srcdir)/win32/Makefile.msvcproj

dist-hook: \
	$(top_builddir)/win32/vs9/gdk3-win32.vcproj	\
	$(top_builddir)/win32/vs9/gdk3-win32.headers

-include $(top_srcdir)/git.mk
