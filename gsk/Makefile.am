include $(top_srcdir)/Makefile.decl

GSK_CFLAG_DEFINES = \
	-DG_LOG_DOMAIN=\"Gsk\"			\
	-DG_LOG_USE_STRUCTURED=1		\
	-DGSK_COMPILATION

AM_CPPFLAGS = \
	$(GSK_CFLAG_DEFINES)			\
	-I$(top_srcdir)				\
	-I$(top_srcdir)/gdk			\
	-I$(top_builddir)			\
	-I$(top_builddir)/gsk			\
	$(GTK_DEBUG_FLAGS)			\
	$(GTK_WARN_FLAGS)			\
	$(GSK_DEP_CFLAGS)

BUILT_SOURCES =

CLEANFILES =
DISTCLEANFILES =

noinst_LTLIBRARIES =

gsk_public_source_h = \
	gskenums.h \
	gskrenderer.h \
	gskrendernode.h \
	gskrendernodeiter.h \
	gsktexture.h \
	gsktypes.h
gsk_private_source_h = \
	gskcairorendererprivate.h \
	gskdebugprivate.h \
	gskgldriverprivate.h \
	gskglprofilerprivate.h \
	gskglrendererprivate.h \
	gskprivate.h \
	gskprofilerprivate.h \
	gskrendererprivate.h \
	gskrendernodeprivate.h \
	gskshaderbuilderprivate.h \
	gsktextureprivate.h
gsk_public_source_c = \
	gskrenderer.c \
	gskrendernode.c \
	gskrendernodeiter.c \
	gsktexture.c
gsk_private_source_c = \
	gskcairorenderer.c \
	gskdebug.c \
	gskgldriver.c \
	gskglprofiler.c \
	gskglrenderer.c \
	gskprivate.c \
	gskprofiler.c \
	gskshaderbuilder.c
gsk_built_source_h = \
	gskenumtypes.h \
	gskresources.h
gsk_built_source_c = \
	gskenumtypes.c \
	gskresources.c

all_sources = \
	$(gsk_public_source_h) \
	$(gsk_public_source_c) \
	$(gsk_private_source_h) \
	$(gsk_private_source_c)

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

libgsk_4_la_SOURCES = $(all_sources)
nodist_libgsk_4_la_SOURCES = $(gsk_built_source_h) $(gsk_built_source_c)
libgsk_4_la_CFLAGS = $(AM_CFLAGS) $(GDK_HIDDEN_VISIBILITY_CFLAGS)
libgsk_4_la_LIBADD = $(GSK_DEP_LIBS) $(top_builddir)/gdk/libgdk-4.la

noinst_LTLIBRARIES += libgsk-4.la

gskincludedir = $(includedir)/gtk-4.0/gsk
gskinclude_HEADERS = $(gsk_public_source_h) gskenumtypes.h gsk.h

-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ENV = CC="$(CC)"
INTROSPECTION_SCANNER_ARGS = \
	--add-include-path=../gdk \
	--add-include-path=../gsk \
	--warn-all
INTROSPECTION_COMPILER_ARGS = \
	--includedir=$(srcdir) \
	--includedir=. \
	--includedir=../gdk

if HAVE_INTROSPECTION

introspection_files = $(gsk_public_source_h) $(gsk_public_source_c)

Gsk-4.0_noinst.gir: libgsk-4.la Makefile
Gsk_noinst_4_0_gir_SCANNERFLAGS = \
	--add-include-path=$(top_builddir)/gdk \
	--include-uninstalled=$(top_builddir)/gdk/Gdk-noinst-4.0.gir \
	--c-include="gsk/gsk.h"
Gsk_noinst_4_0_gir_LIBS = libgsk-4.la
Gsk_noinst_4_0_gir_FILES = $(introspection_files)
Gsk_noinst_4_0_gir_CFLAGS = $(AM_CPPFLAGS) $(GDK_HIDDEN_VISIBILITY_CFLAGS)
Gsk_noinst_4_0_gir_EXPORT_PACKAGES = gsk-4.0
Gsk_noinst_4_0_gir_INCLUDES = GObject-2.0 cairo-1.0 Graphene-1.0
INTROSPECTION_GIRS += Gsk-noinst-4.0.gir

noinst_DATA = $(INTROSPECTION_GIRS)

CLEANFILES += $(INTROSPECTION_GIRS)

endif

# ------------------- MSVC Build Items ----------------
MSVCPROJS = gsk-4

gsk_4_FILES = $(all_sources) $(gsk_built_source_c)
gsk_4_EXCLUDES = dummy
gsk_4_HEADERS_DIR = $(gskincludedir)
gsk_4_HEADERS_INST = $(gskinclude_HEADERS)
gsk_4_HEADERS_EXCLUDES = dummy

include $(top_srcdir)/build/Makefile.msvcproj

$(top_builddir)/build/win32/gsk.enum.headers: Makefile $(gsk_public_source_h)
	-$(RM) $(top_builddir)/build/win32/gsk.enum.headers
	for F in $(gsk_public_source_h); do \
		case $$F in \
		*.h) \
			echo $$F' ^'>>$(top_builddir)/build/win32/gsk.enum.headers \
			;; \
		esac \
	done

if HAVE_INTROSPECTION
# Introspection Items for MSVC
MSVC_INTROSPECT_GIRS = Gsk-4.0.gir

BASE_MSVC_GIR_CFLAGS =			\
	$(GSK_CFLAG_DEFINES)		\
	-I../.. -I../../gdk -I.../../gsk

INTROSPECTION_INTERMEDIATE_ITEMS = \
	$(top_builddir)/build/win32/Gsk-4.0.gir.msvc.introspect	\
	$(top_builddir)/build/win32/Gsk_4_0_gir_list		\
	$(top_builddir)/build/win32/gsk.enum.headers

Gsk_4_0_gir_MSVC_FILES = $(introspection_files)
Gsk_4_0_gir_MSVC_EXPORT_PACKAGES = $(Gsk_noinst_4_0_gir_EXPORT_PACKAGES)
Gsk_4_0_gir_MSVC_INCLUDE_GIRS = $(Gsk_noinst_4_0_gir_INCLUDES)
Gsk_4_0_gir_MSVC_LIBS = gtk-4
Gsk_4_0_gir_MSVC_CFLAGS = $(BASE_MSVC_GIR_CFLAGS)
Gsk_4_0_gir_MSVC_SCANNERFLAGS =			\
	--add-include-path=.			\
	--include-uninstalled=../gdk/Gdk-4.0.gir

include $(top_srcdir)/build/Makefile.msvc-introspection

else
INTROSPECTION_INTERMEDIATE_ITEMS =
endif

dist-hook: \
	$(top_builddir)/build/win32/vs12/gsk-4.vcxproj	\
	$(top_builddir)/build/win32/vs12/gsk-4.vs12.headers	\
	$(top_builddir)/build/win32/gsk.enum.headers	\
	$(INTROSPECTION_INTERMEDIATE_ITEMS)

-include $(top_srcdir)/git.mk
