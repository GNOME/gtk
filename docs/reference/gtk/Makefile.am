## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.6

# The name of the module.
DOC_MODULE=gtk

# The top-level SGML file.
DOC_MAIN_SGML_FILE=gtk-docs.sgml

# Extra options to supply to gtkdoc-scan
SCAN_OPTIONS=--deprecated-guards="GTK_ENABLE_BROKEN|GTK_DISABLE_DEPRECATED" 

# Extra options to pass to gtkdoc-scangobj
SCANGOBJ_OPTIONS=--type-init-func="gtk_type_init(0)" 

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR=../../../gtk

# Used for dependencies
HFILE_GLOB=$(top_srcdir)/gtk/*.h
CFILE_GLOB=$(top_srcdir)/gtk/*.c

# Header files to ignore when scanning
IGNORE_HFILES=				\
	fnmatch.h			\
	gdk-pixbuf-loader.h		\
	gtkcellrendererseptext.h	\
	gtkdebug.h			\
	gtkfilechooserdefault.h		\
	gtkfilechooserentry.h		\
	gtkfilechooserutils.h		\
	gtkfilesystem.h			\
	gtkfilesystemmodel.h		\
	gtkfilesystemunix.h		\
	gtkfilesystemwin32.h		\
	gtkhsv.h			\
	gtkiconthemeparser.h		\
	gtkimmodule.h			\
	gtkintl.h			\
	gtkkeyhash.h			\
	gtkmarshal.h			\
	gtkprivate.h			\
	gtktreeprivate.h		\
	gtkrbtree.h			\
	gtktreedatalist.h		\
	gtktextbtree.h			\
	gtktextdisplay.h		\
	gtktextiterprivate.h		\
	gtktextlayout.h			\
	gtktextmarkprivate.h		\
	gtktextsegment.h		\
	gtktexttagprivate.h		\
	gtktexttypes.h			\
	gtktextutil.h			\
	gtktypebuiltins.h		\
	gtkxembed.h			\
	xdgmime				\
	xembed.h			\
	gtktoggleactionprivate.h

# CFLAGS and LDFLAGS for compiling scan program. Only needed
# if $(DOC_MODULE).types is non-empty.
INCLUDES = \
	-I$(top_srcdir) 	\
	-I$(top_builddir) 	\
	-I$(top_builddir)/gdk	\
	-DGTK_ENABLE_BROKEN	\
	$(GTK_DEBUG_FLAGS)	\
	$(GTK_DEP_CFLAGS)

GTKDOC_LIBS = \
	$(top_builddir)/gdk-pixbuf/libgdk_pixbuf-$(GTK_API_VERSION).la	\
	$(top_builddir)/gdk/$(gdktargetlib)        			\
	$(top_builddir)/gtk/$(gtktargetlib)


# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS=--sgml-mode --output-format=xml

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE)
content_files =					\
	version.xml				\
	running.sgml				\
	building.sgml				\
	changes-1.2.sgml			\
	changes-2.0.sgml			\
	compiling.sgml				\
	framebuffer.sgml			\
	migrating-GtkFileChooser.sgml		\
	objects_grouped.sgml			\
	question_index.sgml			\
	resources.sgml				\
	text_widget.sgml			\
	tree_widget.sgml			\
	windows.sgml				\
	x11.sgml				\
	gtk-query-immodules-2.0.xml

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
	$(top_srcdir)/gtk/stock-icons/stock_jump_to_rtl_24.png		\
	$(top_srcdir)/gtk/stock-icons/stock_align_center_24.png		\
	$(top_srcdir)/gtk/stock-icons/stock_align_justify_24.png	\
	$(top_srcdir)/gtk/stock-icons/stock_align_left_24.png		\
	$(top_srcdir)/gtk/stock-icons/stock_align_right_24.png		\
	$(top_srcdir)/gtk/stock-icons/stock_broken_image_24.png		\
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
	$(top_srcdir)/gtk/stock-icons/stock_redo_rtl_24.png		\
	$(top_srcdir)/gtk/stock-icons/stock_refresh_24.png		\
	$(top_srcdir)/gtk/stock-icons/stock_remove_24.png		\
	$(top_srcdir)/gtk/stock-icons/stock_revert_24.png		\
	$(top_srcdir)/gtk/stock-icons/stock_revert_rtl_24.png		\
	$(top_srcdir)/gtk/stock-icons/stock_save_24.png			\
	$(top_srcdir)/gtk/stock-icons/stock_save_as_24.png		\
	$(top_srcdir)/gtk/stock-icons/stock_colorselector_24.png	\
	$(top_srcdir)/gtk/stock-icons/stock_color_picker_25.png		\
	$(top_srcdir)/gtk/stock-icons/stock_font_24.png			\
	$(top_srcdir)/gtk/stock-icons/stock_sort_ascending_24.png	\
	$(top_srcdir)/gtk/stock-icons/stock_sort_descending_24.png	\
	$(top_srcdir)/gtk/stock-icons/stock_spellcheck_24.png		\
	$(top_srcdir)/gtk/stock-icons/stock_stop_24.png			\
	$(top_srcdir)/gtk/stock-icons/stock_text_strikethrough_24.png	\
	$(top_srcdir)/gtk/stock-icons/stock_undelete_24.png		\
	$(top_srcdir)/gtk/stock-icons/stock_undelete_rtl_24.png		\
	$(top_srcdir)/gtk/stock-icons/stock_text_underline_24.png	\
	$(top_srcdir)/gtk/stock-icons/stock_undo_24.png			\
	$(top_srcdir)/gtk/stock-icons/stock_undo_rtl_24.png		\
	$(top_srcdir)/gtk/stock-icons/stock_yes_20.png			\
	$(top_srcdir)/gtk/stock-icons/stock_zoom_1_24.png		\
	$(top_srcdir)/gtk/stock-icons/stock_zoom_fit_24.png		\
	$(top_srcdir)/gtk/stock-icons/stock_zoom_in_24.png		\
	$(top_srcdir)/gtk/stock-icons/stock_zoom_out_24.png

# Extra options to supply to gtkdoc-fixref
FIXXREF_OPTIONS=--extra-dir=../gdk-pixbuf/html --extra-dir=../gdk/html

include $(top_srcdir)/gtk-doc.make

# Other files to distribute
EXTRA_DIST += version.xml.in

########################################################################

man_MANS = gtk-query-immodules-2.0.1 

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
