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
	gtkalias.h			\
	fnmatch.h			\
	gdk-pixbuf-loader.h		\
	gtkdebug.h			\
	gtkdndcursors.h			\
	gtkentryprivate.h		\
	gtkfilechooserdefault.h		\
	gtkfilechooserembed.h		\
	gtkfilechooserentry.h		\
	gtkfilechooserprivate.h		\
	gtkfilechoosersettings.h	\
	gtkfilechooserutils.h		\
	gtkfilesystem.h			\
	gtkfilesystemmodel.h		\
	gtkfilesystemunix.h		\
	gtkfilesystemwin32.h		\
	gtkhsv.h			\
	gtkiconcache.h			\
	gtkiconthemeparser.h		\
	gtkimmodule.h			\
	gtkintl.h			\
	gtkkeyhash.h			\
	gtkmarshal.h			\
	gtkmnemonichash.h		\
	gtkpathbar.h			\
	gtkplugprivate.h		\
	gtkprintbackend.h		\
	gtkprinteroption.h		\
	gtkprinteroptionset.h		\
	gtkprinteroptionwidget.h	\
	gtkprinter-private.h		\
	gtkprint-win32.h		\
	gtkprintutils.h			\
	gtkprintoperation-private.h	\
	gtkprivate.h			\
	gtkrbtree.h			\
	gtkrecentchooserdefault.h	\
	gtkrecentchooserutils.h		\
	gtkrecentchooserprivate.h	\
	gtksequence.h			\
	gtksocketprivate.h		\
	gtktreedatalist.h		\
	gtktreeprivate.h		\
	gtktextbtree.h			\
	gtktextdisplay.h		\
	gtktextiterprivate.h		\
	gtktextlayout.h			\
	gtktextmarkprivate.h		\
	gtktextsegment.h		\
	gtktexttagprivate.h		\
	gtktexttypes.h			\
	gtktextutil.h			\
	gtktoggleactionprivate.h	\
	gtktrayicon.h			\
	gtktypebuiltins.h		\
	gtkxembed.h			\
	gtkwin32embed.h			\
	gtkwin32embedwidget.h		\
	xdgmime				\
	xembed.h

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
	directfb.sgml				\
	framebuffer.sgml			\
	glossary.xml				\
	migrating-checklist.sgml		\
	migrating-GtkAction.sgml		\
	migrating-GtkComboBox.sgml		\
	migrating-GtkFileChooser.sgml		\
	migrating-GtkIconView.sgml		\
	migrating-GtkAboutDialog.sgml		\
	migrating-GtkColorButton.sgml		\
	migrating-GtkAssistant.sgml		\
	migrating-GtkRecentChooser.sgml 	\
	migrating-GtkLinkButton.sgml		\
	objects_grouped.sgml			\
	osx.sgml				\
	question_index.sgml			\
	resources.sgml				\
	text_widget.sgml			\
	tree_widget.sgml			\
	windows.sgml				\
	x11.sgml				\
	gtk-query-immodules-2.0.xml		\
	gtk-update-icon-cache.xml		\
	visual_index.xml

expand_content_files = 				\
	glossary.xml				\
	migrating-checklist.sgml		\
	migrating-GtkAction.sgml		\
	migrating-GtkComboBox.sgml		\
	migrating-GtkFileChooser.sgml		\
	migrating-GtkIconView.sgml		\
	migrating-GtkAboutDialog.sgml		\
	migrating-GtkColorButton.sgml		\
	migrating-GtkAssistant.sgml		\
	migrating-GtkRecentChooser.sgml 	\
	migrating-GtkLinkButton.sgml		\
	tree_widget.sgml			\
	text_widget.sgml			\
	question_index.sgml

# Images to copy into HTML directory
HTML_IMAGES = \
	$(top_srcdir)/gtk/stock-icons/24/gtk-about.png			\
	$(top_srcdir)/gtk/stock-icons/24/gtk-add.png			\
	$(top_srcdir)/gtk/stock-icons/20/gtk-apply.png			\
	$(top_srcdir)/gtk/stock-icons/24/gtk-bold.png			\
	$(top_srcdir)/gtk/stock-icons/20/gtk-cancel.png			\
	$(top_srcdir)/gtk/stock-icons/24/gtk-cdrom.png			\
	$(top_srcdir)/gtk/stock-icons/24/gtk-clear.png			\
	$(top_srcdir)/gtk/stock-icons/24/gtk-close.png			\
	$(top_srcdir)/gtk/stock-icons/24/gtk-connect.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-convert.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-copy.png			\
	$(top_srcdir)/gtk/stock-icons/24/gtk-cut.png			\
	$(top_srcdir)/gtk/stock-icons/24/gtk-delete.png			\
	$(top_srcdir)/gtk/stock-icons/48/gtk-dialog-authentication.png 	\
	$(top_srcdir)/gtk/stock-icons/48/gtk-dialog-error.png		\
	$(top_srcdir)/gtk/stock-icons/48/gtk-dialog-info.png		\
	$(top_srcdir)/gtk/stock-icons/48/gtk-dialog-question.png	\
	$(top_srcdir)/gtk/stock-icons/48/gtk-dialog-warning.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-directory.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-disconnect.png		\
	$(top_srcdir)/gtk/stock-icons/32/gtk-dnd.png			\
	$(top_srcdir)/gtk/stock-icons/32/gtk-dnd-multiple.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-edit.png			\
	$(top_srcdir)/gtk/stock-icons/24/gtk-execute.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-file.png			\
	$(top_srcdir)/gtk/stock-icons/24/gtk-find.png			\
	$(top_srcdir)/gtk/stock-icons/24/gtk-find-and-replace.png	\
	$(top_srcdir)/gtk/stock-icons/24/gtk-goto-bottom.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-goto-first-ltr.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-goto-first-rtl.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-goto-last-ltr.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-goto-last-rtl.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-goto-top.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-go-back-ltr.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-go-back-rtl.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-go-down.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-go-forward-ltr.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-go-forward-rtl.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-go-up.png			\
	$(top_srcdir)/gtk/stock-icons/24/gtk-harddisk.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-network.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-help.png			\
	$(top_srcdir)/gtk/stock-icons/24/gtk-home.png			\
	$(top_srcdir)/gtk/stock-icons/24/gtk-index.png			\
	$(top_srcdir)/gtk/stock-icons/24/gtk-indent-ltr.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-indent-rtl.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-unindent-ltr.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-unindent-rtl.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-italic.png			\
	$(top_srcdir)/gtk/stock-icons/24/gtk-jump-to-ltr.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-jump-to-rtl.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-justify-center.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-justify-fill.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-justify-left.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-justify-right.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-missing-image.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-new.png			\
	$(top_srcdir)/gtk/stock-icons/20/gtk-no.png			\
	$(top_srcdir)/gtk/stock-icons/20/gtk-ok.png			\
	$(top_srcdir)/gtk/stock-icons/24/gtk-open.png			\
	$(top_srcdir)/gtk/stock-icons/24/gtk-orientation-landscape.png	\
	$(top_srcdir)/gtk/stock-icons/24/gtk-orientation-portrait.png	\
	$(top_srcdir)/gtk/stock-icons/24/gtk-orientation-reverse-landscape.png \
	$(top_srcdir)/gtk/stock-icons/24/gtk-orientation-reverse-portrait.png \
	$(top_srcdir)/gtk/stock-icons/24/gtk-paste.png			\
	$(top_srcdir)/gtk/stock-icons/24/gtk-preferences.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-print.png			\
	$(top_srcdir)/gtk/stock-icons/24/gtk-print-preview.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-properties.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-quit.png			\
	$(top_srcdir)/gtk/stock-icons/24/gtk-redo-ltr.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-redo-rtl.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-refresh.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-remove.png			\
	$(top_srcdir)/gtk/stock-icons/24/gtk-revert-to-saved-ltr.png	\
	$(top_srcdir)/gtk/stock-icons/24/gtk-revert-to-saved-rtl.png	\
	$(top_srcdir)/gtk/stock-icons/24/gtk-floppy.png			\
	$(top_srcdir)/gtk/stock-icons/24/gtk-save.png			\
	$(top_srcdir)/gtk/stock-icons/24/gtk-save-as.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-select-all.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-select-color.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-color-picker.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-font.png			\
	$(top_srcdir)/gtk/stock-icons/24/gtk-sort-ascending.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-sort-descending.png	\
	$(top_srcdir)/gtk/stock-icons/24/gtk-spell-check.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-stop.png			\
	$(top_srcdir)/gtk/stock-icons/24/gtk-strikethrough.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-undelete-ltr.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-undelete-rtl.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-underline.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-undo-ltr.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-undo-rtl.png		\
	$(top_srcdir)/gtk/stock-icons/20/gtk-yes.png			\
	$(top_srcdir)/gtk/stock-icons/24/gtk-zoom-100.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-zoom-fit.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-zoom-in.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-zoom-out.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-media-forward-ltr.png	\
	$(top_srcdir)/gtk/stock-icons/24/gtk-media-forward-rtl.png	\
	$(top_srcdir)/gtk/stock-icons/24/gtk-media-next-ltr.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-media-next-rtl.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-media-pause.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-media-play-ltr.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-media-play-rtl.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-media-previous-ltr.png	\
	$(top_srcdir)/gtk/stock-icons/24/gtk-media-previous-rtl.png	\
	$(top_srcdir)/gtk/stock-icons/24/gtk-media-record.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-media-rewind-ltr.png	\
	$(top_srcdir)/gtk/stock-icons/24/gtk-media-rewind-rtl.png	\
	$(top_srcdir)/gtk/stock-icons/24/gtk-media-stop.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-fullscreen.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-leave-fullscreen.png	\
	$(top_srcdir)/gtk/stock-icons/24/gtk-info.png			\
	$(srcdir)/images/accel-label.png				\
	$(srcdir)/images/assistant.png					\
	$(srcdir)/images/button.png					\
	$(srcdir)/images/check-button.png				\
	$(srcdir)/images/color-button.png				\
	$(srcdir)/images/colorsel.png					\
	$(srcdir)/images/combo-box.png					\
	$(srcdir)/images/combo-box-entry.png				\
	$(srcdir)/images/entry.png					\
	$(srcdir)/images/file-button.png				\
	$(srcdir)/images/filechooser.png				\
	$(srcdir)/images/font-button.png				\
	$(srcdir)/images/fontsel.png					\
	$(srcdir)/images/frame.png					\
	$(srcdir)/images/icon-view.png					\
	$(srcdir)/images/image.png					\
	$(srcdir)/images/label.png					\
	$(srcdir)/images/link-button.png				\
	$(srcdir)/images/list-and-tree.png				\
	$(srcdir)/images/menubar.png					\
	$(srcdir)/images/messagedialog.png				\
	$(srcdir)/images/multiline-text.png				\
	$(srcdir)/images/notebook.png					\
	$(srcdir)/images/panes.png					\
	$(srcdir)/images/pagesetupdialog.png				\
	$(srcdir)/images/printdialog.png				\
	$(srcdir)/images/progressbar.png				\
	$(srcdir)/images/radio-group.png				\
	$(srcdir)/images/recentchooserdialog.png			\
	$(srcdir)/images/scales.png					\
	$(srcdir)/images/scrolledwindow.png				\
	$(srcdir)/images/separator.png					\
	$(srcdir)/images/spinbutton.png					\
	$(srcdir)/images/statusbar.png					\
	$(srcdir)/images/toggle-button.png				\
	$(srcdir)/images/toolbar.png					\
	$(srcdir)/images/window.png

# Extra options to supply to gtkdoc-fixref
FIXXREF_OPTIONS=--extra-dir=../gdk-pixbuf/html --extra-dir=../gdk/html

include $(top_srcdir)/gtk-doc.make

# Other files to distribute
EXTRA_DIST += version.xml.in

########################################################################

man_MANS = gtk-query-immodules-2.0.1 gtk-update-icon-cache.1 

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
