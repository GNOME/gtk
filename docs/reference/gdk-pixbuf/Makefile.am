## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.6

# The name of the module.
DOC_MODULE=gdk-pixbuf

# The top-level SGML file.
DOC_MAIN_SGML_FILE=gdk-pixbuf.sgml

# Extra options to supply to gtkdoc-scan
SCAN_OPTIONS=--source-dir=../../../contrib/gdk-pixbuf-xlib --deprecated-guards="GDK_PIXBUF_ENABLE_BROKEN|GDK_PIXBUF_DISABLE_DEPRECATED"

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR=../../../gdk-pixbuf

# Used for dependencies
HFILE_GLOB=$(top_srcdir)/gdk-pixbuf/*.h $(top_srcdir)/contrib/gdk-pixbuf-xlib/*.h
CFILE_GLOB=$(top_srcdir)/gdk-pixbuf/*.c $(top_srcdir)/contrib/gdk-pixbuf-xlib/*.c

# Header files to ignore when scanning
IGNORE_HFILES=				\
	pixops				\
	gdk-pixbuf-alias.h		\
	gdk-pixbuf-marshal.h		\
	gdk-pixbuf-xlib-private.h 	\
	gdk-pixbuf-i18n.h 		\
	gdk-pixbuf-private.h		\
	io-gif-animation.h		\
	io-ani-animation.h		\
	xpm-color-table.h		\
	test-images.h

INCLUDES = 			\
	-I$(top_srcdir) 	\
	-I$(top_builddir) 	\
	-I$(top_builddir)/gdk	\
	$(GTK_DEBUG_FLAGS)	\
	$(GTK_DEP_CFLAGS)

GTKDOC_LIBS = \
	$(top_builddir)/gdk-pixbuf/libgdk_pixbuf-$(GTK_API_VERSION).la	\
	$(top_builddir)/gdk/$(gdktargetlib)        			\
	$(top_builddir)/gtk/$(gtktargetlib) 				\
	$(GTK_DEP_LIBS)


# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS=--main-sgml-file=$(DOC_MAIN_SGML_FILE) --sgml-mode --source-dir=../../../contrib/gdk-pixbuf-xlib --output-format=xml

# Extra SGML files that are included by DOC_MAIN_SGML_FILE
content_files =				\
	version.xml			\
	gdk-pixbuf-from-drawables.sgml	\
	gdk-pixbuf-rendering.sgml	\
	gdk-pixbuf.sgml			\
	porting-from-imlib.sgml		\
	gdk-pixbuf-csource.xml		\
	gdk-pixbuf-query-loaders.xml

# Images to copy into HTML directory
HTML_IMAGES = composite.png

# Extra options to supply to gtkdoc-fixref
FIXXREF_OPTIONS=

include $(top_srcdir)/gtk-doc.make

# Other files to distribute
EXTRA_DIST += version.xml.in \
	composite.png \
	composite.dia \
	apple-red-1a.png \
	apple-red-2c.png \
	gnome-gmush-1.png

########################################################################

man_MANS = gdk-pixbuf-csource.1 gdk-pixbuf-query-loaders.1 

if ENABLE_MAN

%.1 : %.xml 
	@XSLTPROC@ -nonet http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $<

endif

BUILT_EXTRA_DIST = $(man_MANS)

dist-hook-local: $(BUILT_EXTRA_DIST)
	files='$(BUILT_EXTRA_DIST)';				\
	for f in $$files; do					\
	  if test -f $$f; then d=.; else d=$(srcdir); fi;	\
	  cp $$d/$$f $(distdir) || exit 1; done
