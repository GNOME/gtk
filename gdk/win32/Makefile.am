## Process this file with automake to produce Makefile.in
include $(top_srcdir)/Makefile.decl
include gdk-win32-sources.inc

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

libgdk_win32_la_LIBADD = \
	$(LDADDS) \
	$(NULL)

libgdkinclude_HEADERS = $(GDK_PUBLIC_H_SRCS_WIN32)

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
