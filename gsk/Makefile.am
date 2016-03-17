include $(top_srcdir)/Makefile.decl

AM_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"Gsk\"			\
	-DGSK_COMPILATION			\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/gdk			\
	-I$(top_builddir)			\
	-I$(top_builddir)/gsk			\
	$(GTK_DEBUG_FLAGS)			\
	$(GSK_DEP_CFLAGS)

# libtool stuff: set version and export symbols for resolving
# since automake doesn't support conditionalized libsomething_la_LDFLAGS
# we use the general approach here
LDADD = \
	$(GTK_LINK_FLAGS) \
	-version-info $(LT_VERSION_INFO) \
	-export-dynamic	\
	-rpath $(libdir) \
	$(no_undefined)

BUILT_SOURCES =

CLEANFILES =
DISTCLEANFILES =

lib_LTLIBRARIES =

gsk_public_source_h = \
	gskenums.h \
	gskrenderer.h \
	gskrendernode.h \
	gskrendernodeiter.h \
	gsktypes.h
gsk_private_source_h = \
	gskcairorendererprivate.h \
	gskdebugprivate.h \
	gskglrendererprivate.h \
	gskrendererprivate.h \
	gskrendernodeprivate.h \
	gskprivate.h
gsk_private_source_c = \
	gskprivate.c
gsk_built_source_h = \
	gskenumtypes.h \
	gskresources.h
gsk_built_source_c = \
	gskenumtypes.c \
	gskresources.c
gsk_source_c = \
	gskcairorenderer.c \
	gskdebug.c \
	gskglrenderer.c \
	gskrenderer.c \
	gskrendernode.c \
	gskrendernodeiter.c

all_sources = \
	$(gsk_public_source_h) \
	$(gsk_private_source_h) \
	$(gsk_built_source_h) \
	$(gsk_private_source_c) \
	$(gsk_source_c)

BUILT_SOURCES += $(gsk_built_source_h) $(gsk_built_source_c) gsk.resources.xml

gskenumtypes.h: $(gsk_public_source_h) gskenumtypes.h.template
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $(filter %.template,$^) $(filter-out %.template,$^) > \
	  gskenumtypes.h.tmp && \
	  mv gskenumtypes.h.tmp gskenumtypes.h

gskenumtypes.c: $(gsk_public_source_h) gskenumtypes.c.template
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $(filter %.template,$^) $(filter-out %.template,$^) > \
	  gskenumtypes.c.tmp && \
	  mv gskenumtypes.c.tmp gskenumtypes.c

EXTRA_DIST += gskenumtypes.h.template gskenumtypes.c.template
DISTCLEANFILES += gskenumtypes.h gskenumtypes.c

resource_files = $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(builddir)/gsk.resources.xml)

gsk.resources.xml: Makefile.am
	$(AM_V_GEN) echo "<?xml version='1.0' encoding='UTF-8'?>" > $@; \
	echo "<gresources>" >> $@; \
	echo "  <gresource prefix='/org/gtk/libgsk'>" >> $@; \
	for f in $(top_srcdir)/gsk/resources/glsl/*; do \
	  n=`basename $$f`; \
	  echo "    <file alias='glsl/$$n'>resources/glsl/$$n</file>" >> $@; \
	done; \
	echo "  </gresource>" >> $@; \
	echo "</gresources>" >> $@

gskresources.h: gsk.resources.xml
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $< \
		--target=$@ --sourcedir=$(srcdir) --c-name _gsk --generate-header --manual-register

gskresources.c: gsk.resources.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $< \
		--target=$@ --sourcedir=$(srcdir) --c-name _gsk --generate-source --manual-register

EXTRA_DIST += $(resource_files)
CLEANFILES += gsk.resources.xml
DISTCLEANFILES += gskresources.h gskresources.c

libgsk_3_la_SOURCES = $(all_sources)
nodist_libgsk_3_la_SOURCES = $(gsk_built_source_h) $(gsk_built_source_c)
libgsk_3_la_CFLAGS = $(AM_CFLAGS) $(GDK_HIDDEN_VISIBILITY_CFLAGS)
libgsk_3_la_LIBADD = $(GSK_DEP_LIBS) $(top_builddir)/gdk/libgdk-3.la
libgsk_3_la_LDFLAGS = $(LDADD)

lib_LTLIBRARIES += libgsk-3.la

gskincludedir = $(includedir)/gtk-3.0/gsk
gskinclude_HEADERS = $(gsk_public_source_h) gskenumtypes.h gsk.h

-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ENV = \
	CC="$(CC)"
INTROSPECTION_SCANNER_ARGS = \
	--add-include-path=../gdk \
	--warn-all
INTROSPECTION_COMPILER_ARGS = \
	--includedir=$(srcdir) \
	--includedir=. \
	--includedir=../gdk

if HAVE_INTROSPECTION

introspection_files = $(filter-out $(wildcard *private.h),$(all_sources))

Gsk-3.0.gir: libgsk-3.la Makefile
Gsk_3_0_gir_SCANNERFLAGS = \
	--add-include-path=$(top_builddir)/gdk \
	--include-uninstalled=$(top_builddir)/gdk/Gdk-3.0.gir \
	--c-include="gsk/gsk.h"
Gsk_3_0_gir_LIBS = libgsk-3.la
Gsk_3_0_gir_FILES = $(introspection_files)
Gsk_3_0_gir_CFLAGS = $(AM_CPPFLAGS)
Gsk_3_0_gir_EXPORT_PACKAGES = gsk-3.0
Gsk_3_0_gir_INCLUDES = GObject-2.0 cairo-1.0 Graphene-1.0
INTROSPECTION_GIRS += Gsk-3.0.gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibsdir = $(libdir)/girepository-1.0
typelibs_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelibs_DATA)

endif

-include $(top_srcdir)/git.mk
