## Process this file with automake to produce Makefile.in

# The name of the module.
DOC_MODULE=gtk

# The top-level SGML file.
DOC_MAIN_SGML_FILE=gtk-docs.sgml

# We could alternatively use this for using installed headers.
INCLUDE_DIR=`gtk-config --prefix`/include/gtk


TARGET_DIR=$(HTML_DIR)/$(DOC_MODULE)

CFLAGS=`gtk-config --cflags`
LDFLAGS=`gtk-config --libs`

scan:
	CFLAGS=$(CFLAGS) LDFLAGS=$(LDFLAGS) gtkdoc-scanobj --module=$(DOC_MODULE)
	gtkdoc-scan --module=$(DOC_MODULE) --ignore-headers='gtkintl.h' $(INCLUDE_DIR)/*.h

templates: scan
	gtkdoc-mktmpl --module=$(DOC_MODULE)

sgml:
	gtkdoc-mkdb --module=$(DOC_MODULE)

html:
	if ! test -d html ; then mkdir html ; fi
	-cd html && gtkdoc-mkhtml $(DOC_MODULE) ../$(DOC_MAIN_SGML_FILE)

clean-local:
	rm -f *~ *.bak *.hierarchy *.signals *.args *-unused.txt

maintainer-clean-local: clean
	rm -rf sgml html $(DOC_MODULE)-decl-list.txt $(DOC_MODULE)-decl.txt 

install-data-local:
	install -d -m 0755 $(TARGET_DIR)
	install -m 0644 html/*.html $(TARGET_DIR)
	install -m 0644 html/index.sgml $(TARGET_DIR)
	gtkdoc-fixxref --module=$(DOC_MODULE) --html-dir=$(HTML_DIR)

EXTRA_DIST = \
	$(DOC_MAIN_SGML_FILE) \
	$(DOC_MODULE)-sections.txt \
	gtk-overrides.txt \
	gtk-decl.txt \
	gtk.hierarchy \
	gtk.signals \
	gtk.args \
	gtk.types \
	objects_grouped.sgml

dist-hook:
	mkdir $(distdir)/tmpl
	cp -p tmpl/*.sgml $(distdir)/tmpl

.PHONY : html sgml templates scan
