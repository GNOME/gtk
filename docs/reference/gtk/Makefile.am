## Process this file with automake to produce Makefile.in

# The name of the module.
DOC_MODULE=gtk

# The top-level SGML file.
DOC_MAIN_SGML_FILE=gtk-docs.sgml

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR=../../../gtk

# Extra options to pass to gtkdoc-scangobj
SCANGOBJ_OPTIONS=--type-init-func="gtk_type_init(0)" 

# Extra options to supply to gtkdoc-scan
SCAN_OPTIONS=--deprecated-guards="GTK_ENABLE_BROKEN|GTK_DISABLE_DEPRECATED" 

# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS=--sgml-mode

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
	gtkkeyhash.h		\
	gtkmarshal.h		\
	gtkprivate.h		\
	gtktreeprivate.h	\
	gtkrbtree.h		\
	gtktreedatalist.h	\
	gtktextbtree.h		\
	gtktextdisplay.h	\
	gtktextiterprivate.h	\
	gtktextlayout.h		\
	gtktextmarkprivate.h	\
	gtktextsegment.h	\
	gtktexttagprivate.h	\
	gtktexttypes.h		\
	gtktextutil.h		\
	gtktypebuiltins.h	\
	xembed.h

# Images to copy into HTML directory
HTML_IMAGES = \
	$(top_srcdir)/gtk/stock-icons/stock_add_24.png			\
	$(top_srcdir)/gtk/stock-icons/stock_apply_20.png		\
	$(top_srcdir)/gtk/stock-icons/stock_text_bold_24.png		\
	$(top_srcdir)/gtk/stock-icons/stock_cancel_20.png		\
	$(top_srcdir)/gtk/stock-icons/stock_cdrom_24.png		\
	$(top_srcdir)/gtk/stock-icons/stock_clear_24.png		\
	$(top_srcdir)/gtk/stock-icons/stock_close_24.png		\
	$(top_srcdir)/gtk/stock-icons/stock_convert_24.png		\
	$(top_srcdir)/gtk/stock-icons/stock_copy_24.png			\
	$(top_srcdir)/gtk/stock-icons/stock_cut_24.png			\
	$(top_srcdir)/gtk/stock-icons/stock_trash_24.png		\
	$(top_srcdir)/gtk/stock-icons/stock_dialog_error_48.png		\
	$(top_srcdir)/gtk/stock-icons/stock_dialog_info_48.png		\
	$(top_srcdir)/gtk/stock-icons/stock_dialog_question_48.png	\
	$(top_srcdir)/gtk/stock-icons/stock_dialog_warning_48.png	\
	$(top_srcdir)/gtk/stock-icons/stock_dnd_32.png			\
	$(top_srcdir)/gtk/stock-icons/stock_dnd_multiple_32.png		\
	$(top_srcdir)/gtk/stock-icons/stock_exec_24.png			\
	$(top_srcdir)/gtk/stock-icons/stock_search_24.png		\
	$(top_srcdir)/gtk/stock-icons/stock_search_replace_24.png	\
	$(top_srcdir)/gtk/stock-icons/stock_save_24.png			\
	$(top_srcdir)/gtk/stock-icons/stock_bottom_24.png		\
	$(top_srcdir)/gtk/stock-icons/stock_first_24.png		\
	$(top_srcdir)/gtk/stock-icons/stock_last_24.png			\
	$(top_srcdir)/gtk/stock-icons/stock_top_24.png			\
	$(top_srcdir)/gtk/stock-icons/stock_left_arrow_24.png		\
	$(top_srcdir)/gtk/stock-icons/stock_down_arrow_24.png		\
	$(top_srcdir)/gtk/stock-icons/stock_right_arrow_24.png		\
	$(top_srcdir)/gtk/stock-icons/stock_up_arrow_24.png		\
	$(top_srcdir)/gtk/stock-icons/stock_help_24.png			\
	$(top_srcdir)/gtk/stock-icons/stock_home_24.png			\
	$(top_srcdir)/gtk/stock-icons/stock_index_24.png		\
	$(top_srcdir)/gtk/stock-icons/stock_text_italic_24.png		\
	$(top_srcdir)/gtk/stock-icons/stock_jump_to_24.png		\
	$(top_srcdir)/gtk/stock-icons/stock_align_center_24.png		\
	$(top_srcdir)/gtk/stock-icons/stock_align_justify_24.png	\
	$(top_srcdir)/gtk/stock-icons/stock_align_left_24.png		\
	$(top_srcdir)/gtk/stock-icons/stock_align_right_24.png		\
	$(top_srcdir)/gtk/stock-icons/stock_new_24.png			\
	$(top_srcdir)/gtk/stock-icons/stock_no_20.png			\
	$(top_srcdir)/gtk/stock-icons/stock_ok_20.png			\
	$(top_srcdir)/gtk/stock-icons/stock_open_24.png			\
	$(top_srcdir)/gtk/stock-icons/stock_paste_24.png		\
	$(top_srcdir)/gtk/stock-icons/stock_preferences_24.png		\
	$(top_srcdir)/gtk/stock-icons/stock_print_24.png		\
	$(top_srcdir)/gtk/stock-icons/stock_print_preview_24.png	\
	$(top_srcdir)/gtk/stock-icons/stock_properties_24.png		\
	$(top_srcdir)/gtk/stock-icons/stock_exit_24.png			\
	$(top_srcdir)/gtk/stock-icons/stock_redo_24.png			\
	$(top_srcdir)/gtk/stock-icons/stock_refresh_24.png		\
	$(top_srcdir)/gtk/stock-icons/stock_remove_24.png		\
	$(top_srcdir)/gtk/stock-icons/stock_revert_24.png		\
	$(top_srcdir)/gtk/stock-icons/stock_save_24.png			\
	$(top_srcdir)/gtk/stock-icons/stock_save_as_24.png		\
	$(top_srcdir)/gtk/stock-icons/stock_colorselector_24.png	\
	$(top_srcdir)/gtk/stock-icons/stock_font_24.png			\
	$(top_srcdir)/gtk/stock-icons/stock_sort_ascending_24.png	\
	$(top_srcdir)/gtk/stock-icons/stock_sort_descending_24.png	\
	$(top_srcdir)/gtk/stock-icons/stock_spellcheck_24.png		\
	$(top_srcdir)/gtk/stock-icons/stock_stop_24.png			\
	$(top_srcdir)/gtk/stock-icons/stock_text_strikethrough_24.png	\
	$(top_srcdir)/gtk/stock-icons/stock_undelete_24.png		\
	$(top_srcdir)/gtk/stock-icons/stock_text_underline_24.png	\
	$(top_srcdir)/gtk/stock-icons/stock_undo_24.png			\
	$(top_srcdir)/gtk/stock-icons/stock_yes_20.png			\
	$(top_srcdir)/gtk/stock-icons/stock_zoom_1_24.png		\
	$(top_srcdir)/gtk/stock-icons/stock_zoom_fit_24.png		\
	$(top_srcdir)/gtk/stock-icons/stock_zoom_in_24.png		\
	$(top_srcdir)/gtk/stock-icons/stock_zoom_out_24.png

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE)
content_files = 		\
	running.sgml		\
	building.sgml		\
	changes-1.2.sgml	\
	changes-2.0.sgml	\
	compiling.sgml		\
	framebuffer.sgml	\
	objects_grouped.sgml	\
	question_index.sgml	\
	resources.sgml		\
	text_widget.sgml	\
	tree_widget.sgml	\
	windows.sgml		\
	x11.sgml

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

GTKDOC_LIBS = @STRIP_BEGIN@ 						\
	$(top_builddir)/gdk-pixbuf/libgdk_pixbuf-$(GTK_API_VERSION).la	\
	$(top_builddir)/gdk/@gdktargetlib@        			\
	$(top_builddir)/gtk/@gtktargetlib@ 				\
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
	    CC="$(GTKDOC_CC)" LD="$(GTKDOC_LD)" CFLAGS="$(GTKDOC_CFLAGS)" LDFLAGS="$(GTKDOC_LIBS)" gtkdoc-scangobj $(SCANGOBJ_OPTIONS) --module=$(DOC_MODULE) --output-dir=$(srcdir) ; \
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
	rm -rf $(srcdir)/html 
	mkdir $(srcdir)/html
	cd $(srcdir)/html && gtkdoc-mkhtml $(DOC_MODULE) ../$(DOC_MAIN_SGML_FILE)
	test "x$(HTML_IMAGES)" = "x" || ( cd $(srcdir) && cp $(HTML_IMAGES) html )
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
	(installfiles=`echo $(srcdir)/html/*`; \
	if test "$$installfiles" = '$(srcdir)/html/*'; \
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
	-cp $(srcdir)/html/* $(distdir)/html

.PHONY : dist-hook-local
