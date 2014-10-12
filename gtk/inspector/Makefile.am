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
	action-editor.h			\
	action-editor.c			\
	actions.h			\
	actions.c			\
	classes-list.h			\
	classes-list.c			\
	css-editor.h			\
	css-editor.c			\
	data-list.h			\
	data-list.c			\
	general.h			\
	general.c			\
	gestures.h			\
	gestures.c			\
	init.h				\
	init.c				\
	inspect-button.c		\
	menu.h				\
	menu.c				\
	misc-info.h			\
	misc-info.c			\
	object-hierarchy.h		\
	object-hierarchy.c		\
	object-tree.h			\
	object-tree.c			\
	prop-editor.h			\
	prop-editor.c			\
	prop-list.h			\
	prop-list.c			\
	selector.h			\
	selector.c			\
	style-prop-list.h		\
	style-prop-list.c		\
	resource-list.h			\
	resource-list.c			\
	resources.h			\
	resources.c			\
	signals-list.h			\
	signals-list.c			\
	size-groups.h			\
	size-groups.c			\
	statistics.h			\
	statistics.c			\
	visual.h			\
	visual.c			\
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

if USE_QUARTZ
libgtkinspector_la_CFLAGS += "-xobjective-c"
endif # USE_QUARTZ

libgtkinspector_la_LIBADD = 		\
	$(GTK_DEP_LIBS)			\
	$(INTLLIBS)

libgtkinspector_la_LDFLAGS =		\
	$(LDFLAGS)

templates = 				\
	actions.ui 			\
	classes-list.ui 		\
	css-editor.ui 			\
	data-list.ui 			\
	general.ui 			\
	menu.ui				\
	misc-info.ui			\
	object-hierarchy.ui 		\
	object-tree.ui 			\
	prop-list.ui 			\
	resource-list.ui		\
	selector.ui			\
	signals-list.ui			\
	statistics.ui			\
	style-prop-list.ui		\
	visual.ui			\
	window.ui

template_headers = $(templates:.ui=.ui.h)

%.ui.h : %.ui 
	$(AM_V_GEN) $(top_builddir)/util/extract-strings$(EXEEXT) $< > $@

EXTRA_DIST += 				\
	inspector.gresource.xml 	\
	$(resource_files) 		\
	$(templates)

MAINTAINERCLEANFILES =			\
	resources.c			\
	resources.h

distclean-local:
	if test $(srcdir) != .; then \
	  rm -f $(MAINTAINERCLEANFILES); \
	  rm -f $(template_headers); \
	fi

dist-hook: $(top_builddir)/build/win32/vs9/gtk-inspector.vcproj $(top_builddir)/build/win32/vs10/gtk-inspector.vcxproj $(top_builddir)/build/win32/vs10/gtk-inspector.vcxproj.filters

$(top_builddir)/build/win32/vs9/gtk-inspector.vcproj: $(top_srcdir)/build/win32/vs9/gtk-inspector.vcprojin
	for F in `echo $(libgtkinspector_la_SOURCES) | tr '/' '\\'`; do \
		case $$F in \
		*.c) echo '   <File RelativePath="..\..\..\gtk\inspector\'$$F'" />' \
		     ;; \
		esac; \
	done >libgtk-inspector.sourcefiles
	$(CPP) -P - <$(top_srcdir)/build/win32/vs9/gtk-inspector.vcprojin >$@
	rm libgtk-inspector.sourcefiles

$(top_builddir)/build/win32/vs10/gtk-inspector.vcxproj: $(top_srcdir)/build/win32/vs10/gtk-inspector.vcxprojin
	for F in `echo $(libgtkinspector_la_SOURCES) | tr '/' '\\'`; do \
		case $$F in \
		*.c) echo '   <ClCompile Include="..\..\..\gtk\inspector\'$$F'" />' \
		     ;; \
		esac; \
	done >libgtk-inspector.vs10.sourcefiles
	$(CPP) -P - <$(top_srcdir)/build/win32/vs10/gtk-inspector.vcxprojin >$@
	rm libgtk-inspector.vs10.sourcefiles

$(top_builddir)/build/win32/vs10/gtk-inspector.vcxproj.filters: $(top_srcdir)/build/win32/vs10/gtk-inspector.vcxproj.filtersin
	for F in `echo $(libgtkinspector_la_SOURCES) | tr '/' '\\'`; do \
		case $$F in \
		*.c) echo '   <ClCompile Include="..\..\..\gtk\inspector\'$$F'"><Filter>Source Files</Filter></ClCompile>' \
		     ;; \
		esac; \
	done >libgtk-inspector.vs10.sourcefiles.filters
	$(CPP) -P - <$(top_srcdir)/build/win32/vs10/gtk-inspector.vcxproj.filtersin >$@
	rm libgtk-inspector.vs10.sourcefiles.filters

-include $(top_srcdir)/git.mk
