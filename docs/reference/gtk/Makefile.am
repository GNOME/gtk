## Process this file with automake to produce Makefile.in

# The name of the module.
DOC_MODULE=gtk

# The top-level SGML file.
DOC_MAIN_SGML_FILE=gtk-docs.sgml

# The directory containing the source code
DOC_SOURCE_DIR=$(top_srcdir)/gtk

# Extra options to supply to gtkdoc-fixref
FIXXREF_OPTIONS=--extra-dir=../gdk-pixbuf/html --extra-dir=../gdk/html

# Header files to ignore when scanning
IGNORE_HFILES=			\
	fnmatch.h		\
	gdk-pixbuf-loader.h	\
	gtkdebug.h		\
	gtkhsv.h		\
	gtkimmodule.h		\
	gtkimcontextsimple.h	\
	gtkintl.h		\
	gtkmarshal.h		\
	gtkprivate.h		\
	gtktreeprivate.h	\
	gtkrbtree.h		\
	gtktreedatalist.h	\
	gtktextbtree.h		\
	gtktextchild.h		\
	gtktextdisplay.h	\
	gtktextiterprivate.h	\
	gtktextlayout.h		\
	gtktextmarkprivate.h	\
	gtktextsegment.h	\
	gtktexttagprivate.h	\
	gtktexttypes.h		\
	gtktypebuiltins.h

GTKDOC_CFLAGS = @STRIP_BEGIN@ 				\
	@CFLAGS@					\
	-I$(top_srcdir) 				\
	-I$(top_builddir)/gdk				\
	-DGTK_DISABLE_COMPAT_H				\
	@GTK_DEBUG_FLAGS@				\
	@GTK_XIM_FLAGS@					\
	@GTK_LOCALE_FLAGS@				\
	@PANGO_CFLAGS@					\
	@GLIB_CFLAGS@					\
	@more_cflags@					\
@STRIP_END@

GTKDOC_LIBS = @STRIP_BEGIN@ 				\
	$(top_builddir)/gdk-pixbuf/libgdk_pixbuf-1.3.la	\
	$(top_builddir)/gdk/@gdktargetlib@        	\
	$(top_builddir)/gtk/@gtktargetlib@ 		\
	@more_ldflags@					\
	@more_libs@					\
	@GDK_WLIBS@					\
	@PANGO_LIBS@					\
	@GLIB_LIBS@					\
	@GTK_LIBS_EXTRA@				\
	-lm						\
@STRIP_END@

GTKDOC_CC=$(LIBTOOL) --mode=compile $(CC)
GTKDOC_LD=$(LIBTOOL) --mode=link $(CC)

content_files = 		\
	objects_grouped.sgml	\
	text_widget.sgml

####################################
# Everything below here is generic #
####################################

TARGET_DIR=$(HTML_DIR)/$(DOC_MODULE)

EXTRA_DIST = 				\
	$(DOC_MAIN_SGML_FILE)		\
	$(content_files)		\
	$(DOC_MODULE)-sections.txt	\
	$(DOC_MODULE)-overrides.txt

if ENABLE_GTK_DOC
all-local: html/index.html

$(DOC_MODULE)-decl.txt: 
	$(MAKE) scan

$(tmpl_sources): $(DOC_MODULE)-decl.txt $(DOC_MODULE)-sections.txt $(DOC_MODULE)-overrides.txt
	$(MAKE) templates

sgml/$(DOC_MODULE)-doc.bottom: $(tmpl_sources) $(DOC_MODULE)-decl.txt
	$(MAKE) sgml

html/index.html: sgml/$(DOC_MODULE)-doc.bottom $(DOC_MAIN_SGML_FILE) $(content_files)
	$(MAKE) html
endif

scan:
	(cd $(srcdir) \
	&& CC="$(GTKDOC_CC)" LD="$(GTKDOC_LD)" CFLAGS="$(GTKDOC_CFLAGS)" LDFLAGS="$(GTKDOC_LIBS)" gtkdoc-scangobj --module=$(DOC_MODULE) \
	&& gtkdoc-scan --module=$(DOC_MODULE) --source-dir=$(DOC_SOURCE_DIR) --ignore-headers="$(IGNORE_HFILES)" $(EXTRA_HFILES))

templates: 
	cd $(srcdir) && gtkdoc-mktmpl --module=$(DOC_MODULE)

sgml:
	cd $(srcdir) \
	&& gtkdoc-mkdb --module=$(DOC_MODULE) --source-dir=$(DOC_SOURCE_DIR)

html:
	test -d $(srcdir)/html || mkdir $(srcdir)/html
	-cd $(srcdir)/html && gtkdoc-mkhtml $(DOC_MODULE) ../$(DOC_MAIN_SGML_FILE)
	@echo '-- Fixing Crossreferences' 
	cd $(srcdir) && gtkdoc-fixxref --module-dir=html --html-dir=$(HTML_DIR) $(FIXXREF_OPTIONS)

clean-local:
	rm -f *~ *.bak *.signals *-unused.txt

maintainer-clean-local: clean
	cd $(srcdir) && rm -rf sgml html $(DOC_MODULE)-decl-list.txt $(DOC_MODULE)-decl.txt

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(TARGET_DIR)
	(installfiles=`echo $(srcdir)/html/*.html`; \
	if test "$$installfiles" = '$(srcdir)/html/*.html'; \
	then echo '-- Nothing to install' ; \
	else \
	  for i in $$installfiles; do \
	    echo '-- Installing '$$i ; \
	    $(INSTALL_DATA) $$i $(DESTDIR)$(TARGET_DIR); \
	  done; \
	  echo '-- Installing $(srcdir)/html/index.sgml' ; \
	  $(INSTALL_DATA) $(srcdir)/html/index.sgml $(DESTDIR)$(TARGET_DIR); \
	fi)

#
# Require gtk-doc when making dist
#
if ENABLE_GTK_DOC
dist-check-gtkdoc:
else
dist-check-gtkdoc:
	@echo "*** gtk-doc must be installed and enabled in order to make dist
	@false
endif

dist-hook: dist-check-gtkdoc
	mkdir $(distdir)/html
	mkdir $(distdir)/sgml
	mkdir $(distdir)/tmpl
	-cp $(srcdir)/html/*.html $(srcdir)/html/*.css $(srcdir)/html/index.sgml $(distdir)/html
	-cp $(srcdir)/tmpl/*.sgml $(distdir)/tmpl
	-cp $(srcdir)/sgml/*.sgml $(distdir)/sgml
	-cp $(srcdir)/sgml/*.bottom $(srcdir)/sgml/*.top $(distdir)/sgml

.PHONY : html sgml templates scan
