moduledir = $(libdir)/gtk-3.0/modules

module_LTLIBRARIES = libgtkinspector.la

resource_files = $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/inspector.gresource.xml)
resources.h: inspector.gresource.xml
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/inspector.gresource.xml \
		--target=$@ --sourcedir=$(srcdir) --c-name gtk_inspector --generate-header --manual-register
resources.c: inspector.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/inspector.gresource.xml \
		--target=$@ --sourcedir=$(srcdir) --c-name gtk_inspector --generate-source --manual-register

BUILT_SOURCES = \
	resources.h \
	resources.c

libgtkinspector_la_SOURCES = \
	resources.h \
	resources.c \
	inspect-button.c \
	module.c \
	prop-list.h \
	prop-list.c \
	property-cell-renderer.h \
	property-cell-renderer.c \
	python-hooks.h \
	python-hooks.c \
	python-shell.h \
	python-shell.c \
	widget-tree.h \
	widget-tree.c \
	window.h \
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

libgtkinspector_la_CPPFLAGS = \
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

libgtkinspector_la_LDFLAGS = -avoid-version -module $(no_undefined)

libgtkinspector_la_LIBADD = \
	$(top_builddir)/gtk/libgtk-3.la \
	$(GTK_DEP_LIBS)

EXTRA_DIST= \
	$(resource_files)

-include $(top_srcdir)/git.mk
