## Process this file with automake to produce Makefile.in

# The name of the module.
DOC_MODULE=gtk

# The top-level SGML file.
DOC_MAIN_SGML_FILE=gtk-docs.sgml

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR=../../../gtk

# Extra options to supply to gtkdoc-scan
SCAN_OPTIONS=--deprecated-guards="GTK_ENABLE_BROKEN|GTK_DISABLE_DEPRECATED" 

# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS=

# Extra options to supply to gtkdoc-fixref
FIXXREF_OPTIONS=--extra-dir=../gdk-pixbuf/html --extra-dir=../gdk/html

# Used for dependencies
HFILE_GLOB=$(top_srcdir)/gtk/*.h
CFILE_GLOB=$(top_srcdir)/gtk/*.c

# Header files to ignore when scanning
IGNORE_HFILES=			\
	fnmatch.h		\
	gdk-pixbuf-loader.h	\
	gtkdebug.h		\
	gtkhsv.h		\
	gtkimmodule.h		\
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
	gtktypebuiltins.h	\
	xembed.h

# Images to copy into HTML directory
HTML_IMAGES = \
	$(top_srcdir)/gtk/stock-icons/stock_add.png			\
	$(top_srcdir)/gtk/stock-icons/stock_button_apply.png		\
	$(top_srcdir)/gtk/stock-icons/stock_text_bold.png		\
	$(top_srcdir)/gtk/stock-icons/stock_button_cancel.png		\
	$(top_srcdir)/gtk/stock-icons/stock_cdrom.png			\
	$(top_srcdir)/gtk/stock-icons/stock_clear.png			\
	$(top_srcdir)/gtk/stock-icons/stock_close.png			\
	$(top_srcdir)/gtk/stock-icons/stock_convert.png			\
	$(top_srcdir)/gtk/stock-icons/stock_menu_copy.png		\
	$(top_srcdir)/gtk/stock-icons/stock_menu_cut.png		\
	$(top_srcdir)/gtk/stock-icons/stock_menu_trash.png		\
	$(top_srcdir)/gtk/stock-icons/dialog_error.png			\
	$(top_srcdir)/gtk/stock-icons/dialog_info.png			\
	$(top_srcdir)/gtk/stock-icons/dialog_question.png		\
	$(top_srcdir)/gtk/stock-icons/dialog_warning.png		\
	$(top_srcdir)/gtk/stock-icons/stock_dnd.png			\
	$(top_srcdir)/gtk/stock-icons/stock_dnd_multiple.png		\
	$(top_srcdir)/gtk/stock-icons/stock_exec.png			\
	$(top_srcdir)/gtk/stock-icons/stock_search.png			\
	$(top_srcdir)/gtk/stock-icons/stock_search_replace.png		\
	$(top_srcdir)/gtk/stock-icons/stock_save.png			\
	$(top_srcdir)/gtk/stock-icons/stock_bottom.png			\
	$(top_srcdir)/gtk/stock-icons/stock_first.png			\
	$(top_srcdir)/gtk/stock-icons/stock_last.png			\
	$(top_srcdir)/gtk/stock-icons/stock_top.png			\
	$(top_srcdir)/gtk/stock-icons/stock_left_arrow.png		\
	$(top_srcdir)/gtk/stock-icons/stock_down_arrow.png		\
	$(top_srcdir)/gtk/stock-icons/stock_right_arrow.png		\
	$(top_srcdir)/gtk/stock-icons/stock_up_arrow.png		\
	$(top_srcdir)/gtk/stock-icons/stock_help.png			\
	$(top_srcdir)/gtk/stock-icons/stock_home.png			\
	$(top_srcdir)/gtk/stock-icons/stock_index.png			\
	$(top_srcdir)/gtk/stock-icons/stock_text_italic.png		\
	$(top_srcdir)/gtk/stock-icons/stock_jump_to.png			\
	$(top_srcdir)/gtk/stock-icons/stock_align_center.png		\
	$(top_srcdir)/gtk/stock-icons/stock_align_justify.png		\
	$(top_srcdir)/gtk/stock-icons/stock_align_left.png		\
	$(top_srcdir)/gtk/stock-icons/stock_align_right.png		\
	$(top_srcdir)/gtk/stock-icons/dialog_error.png			\
	$(top_srcdir)/gtk/stock-icons/stock_new.png			\
	$(top_srcdir)/gtk/stock-icons/stock_button_no.png		\
	$(top_srcdir)/gtk/stock-icons/stock_button_ok.png		\
	$(top_srcdir)/gtk/stock-icons/stock_open.png			\
	$(top_srcdir)/gtk/stock-icons/stock_paste.png			\
	$(top_srcdir)/gtk/stock-icons/stock_preferences.png		\
	$(top_srcdir)/gtk/stock-icons/stock_print.png			\
	$(top_srcdir)/gtk/stock-icons/stock_print_preview.png		\
	$(top_srcdir)/gtk/stock-icons/stock_properties.png		\
	$(top_srcdir)/gtk/stock-icons/stock_exit.png			\
	$(top_srcdir)/gtk/stock-icons/stock_redo.png			\
	$(top_srcdir)/gtk/stock-icons/stock_refresh.png			\
	$(top_srcdir)/gtk/stock-icons/stock_remove.png			\
	$(top_srcdir)/gtk/stock-icons/stock_revert.png			\
	$(top_srcdir)/gtk/stock-icons/stock_save.png			\
	$(top_srcdir)/gtk/stock-icons/stock_save_as.png			\
	$(top_srcdir)/gtk/stock-icons/stock_colorselector.png		\
	$(top_srcdir)/gtk/stock-icons/stock_font.png			\
	$(top_srcdir)/gtk/stock-icons/stock_sort_ascending.png		\
	$(top_srcdir)/gtk/stock-icons/stock_sort_descending.png		\
	$(top_srcdir)/gtk/stock-icons/stock_spellcheck.png		\
	$(top_srcdir)/gtk/stock-icons/stock_stop.png			\
	$(top_srcdir)/gtk/stock-icons/stock_text_strikeout.png		\
	$(top_srcdir)/gtk/stock-icons/stock_undelete.png		\
	$(top_srcdir)/gtk/stock-icons/stock_text_underline.png		\
	$(top_srcdir)/gtk/stock-icons/stock_undo.png			\
	$(top_srcdir)/gtk/stock-icons/stock_button_yes.png		\
	$(top_srcdir)/gtk/stock-icons/stock_zoom_1.png			\
	$(top_srcdir)/gtk/stock-icons/stock_zoom_fit.png		\
	$(top_srcdir)/gtk/stock-icons/stock_zoom_in.png			\
	$(top_srcdir)/gtk/stock-icons/stock_zoom_out.png

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE)
content_files = 		\
	building.sgml		\
	compiling.sgml		\
	framebuffer.sgml	\
	objects_grouped.sgml	\
	resources.sgml		\
	text_widget.sgml	\
	tree_widget.sgml

# Other files to distribute
extra_files =

# CFLAGS and LDFLAGS for compiling scan program. Only needed
# if $(DOC_MODULE).types is non-empty.
GTKDOC_CFLAGS = @STRIP_BEGIN@ 				\
	@CFLAGS@					\
	-I$(top_srcdir) 				\
	-I$(top_builddir) 				\
	-I$(top_builddir)/gdk				\
	-DGTK_ENABLE_BROKEN				\
	@GTK_DEBUG_FLAGS@				\
	@GTK_DEP_CFLAGS@				\
@STRIP_END@

GTKDOC_LIBS = @STRIP_BEGIN@ 				\
	$(top_builddir)/gdk-pixbuf/libgdk_pixbuf-1.3.la	\
	$(top_builddir)/gdk/@gdktargetlib@        	\
	$(top_builddir)/gtk/@gtktargetlib@ 		\
@STRIP_END@

GTKDOC_CC=$(LIBTOOL) --mode=compile $(CC)
GTKDOC_LD=$(LIBTOOL) --mode=link $(CC)


####################################
# Everything below here is generic #
####################################

# We set GPATH here; this gives us semantics for GNU make
# which are more like other make's VPATH, when it comes to
# whether a source that is a target of one rule is then
# searched for in VPATH/GPATH.
#
GPATH = $(srcdir)

TARGET_DIR=$(HTML_DIR)/$(DOC_MODULE)

EXTRA_DIST = 				\
	$(content_files)		\
	$(extra_files)			\
	$(HTML_IMAGES)			\
	$(DOC_MAIN_SGML_FILE)		\
	$(DOC_MODULE).types		\
	$(DOC_MODULE)-sections.txt	\
	$(DOC_MODULE)-overrides.txt

DOC_STAMPS=scan-build.stamp tmpl-build.stamp sgml-build.stamp html-build.stamp \
	   $(srcdir)/tmpl.stamp $(srcdir)/sgml.stamp $(srcdir)/html.stamp

SCANOBJ_FILES = 		\
	$(DOC_MODULE).args 	\
	$(DOC_MODULE).hierarchy \
	$(DOC_MODULE).signals

if ENABLE_GTK_DOC
all-local: html-build.stamp

#### scan ####

scan-build.stamp: $(HFILE_GLOB)
	@echo '*** Scanning header files ***'
	if grep -l '^..*$$' $(srcdir)/$(DOC_MODULE).types > /dev/null ; then \
	    CC="$(GTKDOC_CC)" LD="$(GTKDOC_LD)" CFLAGS="$(GTKDOC_CFLAGS)" LDFLAGS="$(GTKDOC_LIBS)" gtkdoc-scangobj --module=$(DOC_MODULE) --output-dir=$(srcdir) ; \
	else \
	    cd $(srcdir) ; \
	    for i in $(SCANOBJ_FILES) ; do \
               test -f $$i || touch $$i ; \
	    done \
	fi
	cd $(srcdir) && \
	  gtkdoc-scan --module=$(DOC_MODULE) --source-dir=$(DOC_SOURCE_DIR) --ignore-headers="$(IGNORE_HFILES)" $(SCAN_OPTIONS) $(EXTRA_HFILES)
	touch scan-build.stamp

$(DOC_MODULE)-decl.txt $(SCANOBJ_FILES): scan-build.stamp
	@true

#### templates ####

tmpl-build.stamp: $(DOC_MODULE)-decl.txt $(SCANOBJ_FILES) $(DOC_MODULE)-sections.txt $(DOC_MODULE)-overrides.txt
	@echo '*** Rebuilding template files ***'
	cd $(srcdir) && gtkdoc-mktmpl --module=$(DOC_MODULE)
	touch tmpl-build.stamp

tmpl.stamp: tmpl-build.stamp
	@true

#### sgml ####

sgml-build.stamp: tmpl.stamp $(CFILE_GLOB) $(srcdir)/tmpl/*.sgml
	@echo '*** Building SGML ***'
	cd $(srcdir) && \
	gtkdoc-mkdb --module=$(DOC_MODULE) --source-dir=$(DOC_SOURCE_DIR) $(MKDB_OPTIONS)
	touch sgml-build.stamp

sgml.stamp: sgml-build.stamp
	@true

#### html ####

html-build.stamp: sgml.stamp $(DOC_MAIN_SGML_FILE) $(content_files)
	@echo '*** Building HTML ***'
	test -d $(srcdir)/html || mkdir $(srcdir)/html
	cd $(srcdir)/html && gtkdoc-mkhtml $(DOC_MODULE) ../$(DOC_MAIN_SGML_FILE)
	images="$(HTML_IMAGES)" ; 		\
	for i in $$images ; do			\
	  cp $$i html ;  			\
	done
	@echo '-- Fixing Crossreferences' 
	cd $(srcdir) && gtkdoc-fixxref --module-dir=html --html-dir=$(HTML_DIR) $(FIXXREF_OPTIONS)
	touch html-build.stamp
endif

##############

clean-local:
	rm -f *~ *.bak $(SCANOBJ_FILES) *-unused.txt $(DOC_STAMPS)

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
	@echo "*** gtk-doc must be installed and enabled in order to make dist"
	@false
endif

dist-hook: dist-check-gtkdoc dist-hook-local
	mkdir $(distdir)/tmpl
	mkdir $(distdir)/sgml
	mkdir $(distdir)/html
	-cp $(srcdir)/tmpl/*.sgml $(distdir)/tmpl
	-cp $(srcdir)/sgml/*.sgml $(distdir)/sgml
	cp $(srcdir)/html/index.sgml $(distdir)/html
	-cp $(srcdir)/html/*.html $(srcdir)/html/*.css $(distdir)/html

	images="$(HTML_IMAGES)" ;    	      \
	for i in $$images ; do		      \
	  cp $(srcdir)/$$i $(distdir)/html ;  \
	done

.PHONY : dist-hook-local
