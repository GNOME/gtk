moduledir = $(libdir)/gtk-3.0/modules

module_LTLIBRARIES = libgtkparasite.la

resource_files = $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/parasite.gresource.xml)
resources.h: parasite.gresource.xml
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/parasite.gresource.xml \
		--target=$@ --sourcedir=$(srcdir) --c-name parasite --generate-header --manual-register
resources.c: parasite.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/parasite.gresource.xml \
		--target=$@ --sourcedir=$(srcdir) --c-name parasite --generate-source --manual-register

BUILT_SOURCES = \
	resources.h \
	resources.c

libgtkparasite_la_SOURCES = \
	resources.h \
	resources.c \
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

EXTRA_DIST= \
	$(resource_files)

-include $(top_srcdir)/git.mk
