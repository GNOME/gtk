## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.7

# The name of the module.
DOC_MODULE=gail-libgail-util3

# The top-level SGML file.
DOC_MAIN_SGML_FILE=gail-libgail-util-docs.sgml

# The directory containing the source code (if it contains documentation).
DOC_SOURCE_DIR=$(top_srcdir)/libgail-util

# Used for dependencies
HFILE_GLOB = $(top_srcdir)/libgail-util/*.h
CFILE_GLOB = $(top_srcdir)/libgail-util/*.c

# CFLAGS and LDFLAGS for compiling scan program. Only needed
# if $(DOC_MODULE).types is non-empty.
AM_CPPFLAGS =                   \
        -I$(top_srcdir)         \
        -I$(top_builddir)       \
        $(DEP_CFLAGS)

GTKDOC_LIBS = $(top_builddir)/libgail-util/libgailutil-3.la

# gtkdoc-mkdb related varaibles
MKDB_OPTIONS =
content_files =		\
	version.xml

HTML_IMAGES =

include $(top_srcdir)/gtk-doc.make

# Other files to distribute
EXTRA_DIST += version.xml.in

-include $(top_srcdir)/git.mk
