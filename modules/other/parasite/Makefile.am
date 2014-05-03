moduledir = $(libdir)/gtk-3.0/modules

module_LTLIBRARIES = libgtkparasite.la

libgtkparasite_la_SOURCES = \
	inspect-button.c \
	module.c \
	parasite.h \
	prop-list.h \
	prop-list.c \
	property-cell-renderer.c \
	property-cell-renderer.h \
	python-hooks.c \
	python-hooks.h \
	python-shell.c \
	python-shell.h \
	widget-tree.h \
	widget-tree.c \
	window.c \
	button-path.h \
	button-path.c \
	classes-list.h \
	classes-list.c \
	css-editor.h \
	css-editor.c \
	object-hierarchy.h \
	object-hierarchy.c \
	themes.h \
	themes.c

libgtkparasite_la_CPPFLAGS = \
	-I$(top_srcdir) 		\
	-I$(top_srcdir)/gtk 		\
	-I$(top_builddir)/gtk 		\
	-I$(top_srcdir)/gdk 		\
	-I$(top_builddir)/gdk 		\
	-DGTK_DATADIR=\"$(datadir)\"	\
	-DGTK_COMPILATION 		\
	$(GTK_DEP_CFLAGS)		\
	$(GTK_DEBUG_FLAGS)

if PLATFORM_WIN32
no_undefined = -no-undefined
endif

libgtkparasite_la_LDFLAGS = -avoid-version -module $(no_undefined)

libgtkparasite_la_LIBADD = \
	$(top_builddir)/gtk/libgtk-3.la \
	$(GTK_DEP_LIBS)

-include $(top_srcdir)/git.mk
