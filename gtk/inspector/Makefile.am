include $(top_srcdir)/Makefile.decl

noinst_LTLIBRARIES = libgtkinspector.la

resource_files = $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/inspector.gresource.xml)
resources.h: inspector.gresource.xml
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/inspector.gresource.xml \
		--target=$@ --sourcedir=$(srcdir) --c-name gtk_inspector --generate-header --manual-register
resources.c: inspector.gresource.xml $(resource_files) $(template_headers)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/inspector.gresource.xml \
		--target=$@ --sourcedir=$(srcdir) --c-name gtk_inspector --generate-source --manual-register

BUILT_SOURCES = 			\
	resources.h			\
	resources.c

libgtkinspector_la_SOURCES = 		\
	actions.h			\
	actions.c			\
	button-path.h			\
	button-path.c			\
	classes-list.h			\
	classes-list.c			\
	css-editor.h			\
	css-editor.c			\
	data-list.h			\
	data-list.c			\
	general.h			\
	general.c			\
	init.h				\
	init.c				\
	inspect-button.c		\
	object-hierarchy.h		\
	object-hierarchy.c		\
	prop-editor.h			\
	prop-editor.c			\
	prop-list.h			\
	prop-list.c			\
	python-hooks.h			\
	python-hooks.c			\
	python-shell.h			\
	python-shell.c			\
	resource-list.h			\
	resource-list.c			\
	resources.h			\
	resources.c			\
	signals-list.h			\
	signals-list.c			\
	visual.h			\
	visual.c			\
	widget-tree.h			\
	widget-tree.c			\
	window.h			\
	window.c

libgtkinspector_la_CPPFLAGS = 		\
	-I$(top_srcdir) 		\
	-I$(top_srcdir)/gtk 		\
	-I$(top_builddir)/gtk 		\
	-I$(top_srcdir)/gdk 		\
	-I$(top_builddir)/gdk 		\
	-DGTK_VERSION=\"$(GTK_VERSION)\"\
	-DGTK_DATADIR=\"$(datadir)\"	\
	-DGTK_COMPILATION 		\
	$(AM_CPPFLAGS)

libgtkinspector_la_CFLAGS =		\
	$(GDK_HIDDEN_VISIBILITY_CFLAGS)	\
	$(GTK_DEP_CFLAGS)		\
	$(GTK_DEBUG_FLAGS)		\
	$(AM_CFLAGS)

libgtkinspector_la_LIBADD = 		\
	$(GTK_DEP_LIBS)			\
	$(INTLLIBS)

libgtkinspector_la_LDFLAGS =		\
	$(LDFLAGS)

templates = 				\
	actions.ui 			\
	button-path.ui 			\
	classes-list.ui 		\
	css-editor.ui 			\
	data-list.ui 			\
	general.ui 			\
	object-hierarchy.ui 		\
	prop-list.ui 			\
	resource-list.ui		\
	signals-list.ui			\
	visual.ui			\
	widget-tree.ui 			\
	window.ui

template_headers = $(templates:.ui=.ui.h)

BUILT_SOURCES += $(template_headers)

extract_strings = $(top_builddir)/gtk/extract-strings$(BUILD_EXEEXT)

%.ui.h: %.ui $(extract_strings)
	$(AM_V_GEN) $(extract_strings) $< > $@

EXTRA_DIST += 				\
	inspector.gresource.xml 	\
	$(resource_files) 		\
	$(templates)

-include $(top_srcdir)/git.mk
