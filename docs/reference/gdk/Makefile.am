## Process this file with automake to produce Makefile.in

# The name of the module.
DOC_MODULE=gdk

# The top-level SGML file.
DOC_MAIN_SGML_FILE=gdk-docs.sgml

# The directory containing the source code (if it contains documentation).
DOC_SOURCE_DIR=$(top_srcdir)/gdk

# Extra options to supply to gtkdoc-fixref
FIXXREF_OPTIONS=--extra-dir=../gdk-pixbuf/html

# Header files to ignore when scanning
IGNORE_HFILES=			\
	gdkkeysyms.h		\
	gdkinternals.h		\
	gdkprivate.h		\
	gdkpoly-generic.h	\
	gdkregion-generic.h	\
	linux-fb 		\
	nanox 			\
	win32 			\
	x11

# Extra files to add when scanning
EXTRA_HFILES=			\
	$(top_srcdir)/gdk/x11/gdkx.h 

tmpl_sources =				\
	tmpl/color_contexts.sgml	\
	tmpl/colors.sgml		\
	tmpl/cursors.sgml		\
	tmpl/dnd.sgml			\
	tmpl/drawing.sgml		\
	tmpl/event_structs.sgml		\
	tmpl/events.sgml		\
	tmpl/fonts.sgml			\
	tmpl/gcs.sgml			\
	tmpl/gdk-unused.sgml		\
	tmpl/general.sgml		\
	tmpl/images.sgml		\
	tmpl/input.sgml			\
	tmpl/input_contexts.sgml	\
	tmpl/input_devices.sgml		\
	tmpl/input_methods.sgml		\
	tmpl/keys.sgml			\
	tmpl/pango_interaction.sgml	\
	tmpl/pixbufs.sgml		\
	tmpl/pixmaps.sgml		\
	tmpl/properties.sgml		\
	tmpl/regions.sgml		\
	tmpl/rgb.sgml			\
	tmpl/selections.sgml		\
	tmpl/threads.sgml		\
	tmpl/visuals.sgml		\
	tmpl/windows.sgml		\
	tmpl/x_interaction.sgml

# Extra SGML files that are included by DOC_MAIN_SGML_FILE
content_files =


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

html/index.html: sgml/$(DOC_MODULE)-doc.bottom $(DOC_MAIN_SGML_FILE) $(content_fiels)
	$(MAKE) html
endif

scan:
	-(cd $(srcdir) \
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
	gtkdoc-fixxref --module-dir=html --html-dir=$(HTML_DIR) $(FIXXREF_OPTIONS)

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
	-cp $(srcdir)/html/*.html $(srcdir)/html/*.css $(distdir)/html
	-cp $(srcdir)/tmpl/*.sgml $(distdir)/tmpl
	-cp $(srcdir)/sgml/*.sgml $(distdir)/sgml
	-cp $(srcdir)/sgml/*.bottom $(srcdir)/sgml/*.top $(distdir)/sgml

.PHONY : html sgml templates scan
