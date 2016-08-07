## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.6

# The name of the module.
DOC_MODULE = gsk3

# The top-level SGML file.
DOC_MAIN_SGML_FILE = gsk-docs.xml

# Extra options to supply to gtkdoc-scan
SCAN_OPTIONS = --deprecated-guards=GDK_DISABLE_DEPRECATED \
               --ignore-decorators=G_GNUC_WARN_UNUSED_RESULT

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR = $(top_srcdir)/gsk

# Used for dependencies
HFILE_GLOB = $(top_srcdir)/gsk/*.h
CFILE_GLOB = $(top_srcdir)/gsk/*.c

# Header files to ignore when scanning
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

# Extra files to add when scanning
EXTRA_HFILES =

# CFLAGS and LDFLAGS for compiling scan program. Only needed
# if $(DOC_MODULE).types is non-empty.
AM_CPPFLAGS = 			\
	-I$(top_srcdir)		\
	-I$(top_builddir)	\
	-I$(top_builddir)/gsk	\
	$(GTK_DEBUG_FLAGS)	\
	$(GDK_DEP_CFLAGS)

GTKDOC_LIBS = $(top_builddir)/gsk/libgsk-3.la $(GDK_DEP_LIBS)

# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS=--output-format=xml --name-space=gsk

# Extra SGML files that are included by DOC_MAIN_SGML_FILE
content_files = 		\
	version.xml

# Images to copy into HTML directory
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
