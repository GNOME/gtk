## Process this file with automake to produce Makefile.in

# The name of the module.
DOC_MODULE=gdk-pixbuf

# The top-level SGML file.
DOC_MAIN_SGML_FILE=gdk-pixbuf.sgml

# The directory containing the source code (if it contains documentation).
DOC_SOURCE_DIR=$(GDK_PIXBUF_DIR)/src

CFLAGS=`gnome-config --cflags gnomeui gdk_pixbuf`
LDFLAGS=`gnome-config --libs gnomeui gdk_pixbuf`

HTML_DIR=$(datadir)/gnome/html

TARGET_DIR=$(HTML_DIR)/$(DOC_MODULE)

tmpl_sources =					\
	tmpl/creating.sgml			\
	tmpl/file-loading.sgml			\
	tmpl/gdk-pixbuf-io.sgml			\
	tmpl/gdk-pixbuf-loader.sgml		\
	tmpl/gdk-pixbuf-unused.sgml		\
	tmpl/gdk-pixbuf.sgml			\
	tmpl/gnome-canvas-pixbuf.sgml		\
	tmpl/refcounting.sgml			\
	tmpl/rendering.sgml

gdk_pixbuf_docdir = $(HTML_DIR)
gdk_pixbuf_doc_DATA =			\
	gdk-pixbuf.html			\
	gdk-pixbuf.hierarchy		\
	gdk-pixbuf.types		\
	gdk-pixbuf-decl.txt		\
	gdk-pixbuf-sections.txt

EXTRA_DIST = $(gdk_pixbuf_doc_DATA)

if ENABLE_GTK_DOC
gdk-pixbuf.html: html/book1.html
	-cd $(srcdir) && cp html/book1.html gdk-pixbuf.html
else
gdk-pixbuf.html:
endif

html/book1.html: sgml/gdk-pixbuf-doc.bottom
	$(MAKE) html

sgml/gdk-pixbuf-doc.bottom: $(tmpl_sources)
	$(MAKE) sgml

scan:
	-(cd $(srcdir) \
	&& env CFLAGS=$(CFLAGS) LDFLAGS=$(LDFLAGS) \
		 gtkdoc-scanobj --module=$(DOC_MODULE) \
	&& gtkdoc-scan --module=$(DOC_MODULE) --source-dir=$(DOC_SOURCE_DIR) )

templates: scan
	cd $(srcdir) && gtkdoc-mktmpl --module=$(DOC_MODULE)

sgml:
	cd $(srcdir) \
	&& gtkdoc-mkdb --module=$(DOC_MODULE) --source-dir=$(DOC_SOURCE_DIR)

html:
	test -d $(srcdir)/html || mkdir $(srcdir)/html
	-cd $(srcdir)/html && gtkdoc-mkhtml $(DOC_MODULE) ../$(DOC_MAIN_SGML_FILE)

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
	  echo '-- Fixing Crossreferences' ; \
	  gtkdoc-fixxref --module=$(DOC_MODULE) --html-dir=$(HTML_DIR)|| true; \
	fi)

dist-hook:
	mkdir $(distdir)/html
	mkdir $(distdir)/sgml
	mkdir $(distdir)/tmpl
	-cp $(srcdir)/html/*.html $(srcdir)/html/*.css $(distdir)/html
	-cp $(srcdir)/tmpl/*.sgml $(distdir)/tmpl
	-cp $(srcdir)/sgml/*.sgml $(distdir)/sgml
	-cp $(srcdir)/sgml/*.bottom $(srcdir)/sgml/*.top $(distdir)/sgml

.PHONY : html sgml templates scan
