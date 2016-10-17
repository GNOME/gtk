## Process this file with automake to produce Makefile.in
DOC_MODULE = gsk4
DOC_MAIN_SGML_FILE = gsk-docs.xml

SCAN_OPTIONS = --deprecated-guards=GDK_DISABLE_DEPRECATED \
               --ignore-decorators=G_GNUC_WARN_UNUSED_RESULT

DOC_SOURCE_DIR = $(top_srcdir)/gsk

HFILE_GLOB = $(top_srcdir)/gsk/*.h
CFILE_GLOB = $(top_srcdir)/gsk/*.c

IGNORE_HFILES =				\
	gskcairorendererprivate.h	\
	gskdebugprivate.h		\
	gskdriverprivate.h		\
	gskprofilerprivate.h		\
	gskglrendererprivate.h		\
	gskprivate.h			\
	gskprofilerprivate.h		\
	gskrendererprivate.h		\
	gskrendernodeprivate.h		\
	gskshaderbiulderprivate.h

EXTRA_HFILES =

AM_CPPFLAGS = 			\
	-I$(top_srcdir)		\
	-I$(top_builddir)	\
	-I$(top_builddir)/gsk	\
	$(GTK_DEBUG_FLAGS)	\
	$(GDK_DEP_CFLAGS)

GTKDOC_LIBS = $(top_builddir)/gtk/libgtk-4.la $(GDK_DEP_LIBS)

MKDB_OPTIONS=--output-format=xml --name-space=gsk

content_files = 		\
	version.xml

HTML_IMAGES =

if ENABLE_DOC_CROSS_REFERENCES
# Extra options to supply to gtkdoc-fixref
FIXXREF_OPTIONS= \
	--extra-dir=$(GLIB_PREFIX)/share/gtk-doc/html/gobject \
	--extra-dir=$(GLIB_PREFIX)/share/gtk-doc/html/glib \
	--extra-dir=$(CAIRO_PREFIX)/share/gtk-doc/html/cairo
endif

include $(top_srcdir)/gtk-doc.make

# Other files to distribute
EXTRA_DIST += version.xml.in

if ENABLE_GTK_DOC
TESTS_ENVIRONMENT = cd $(srcdir) && \
  DOC_MODULE=$(DOC_MODULE) DOC_MAIN_SGML_FILE=$(DOC_MAIN_SGML_FILE) \
  SRCDIR=$(abs_srcdir) BUILDDIR=$(abs_builddir)
#TESTS = $(GTKDOC_CHECK)
endif

-include $(top_srcdir)/git.mk
