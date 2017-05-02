## Process this file with automake to produce Makefile.in
include $(top_srcdir)/Makefile.decl

AUTOMAKE_OPTIONS = 1.6

# The name of the module.
DOC_MODULE=gtk2

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
	gtkdebug.h			\
	gtkbuilderprivate.h		\
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
	gtkiconcache.h			\
        gtkiconcachevalidator.h         \
	gtkiconthemeparser.h		\
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
	gtkquery.h			\
	gtkrbtree.h			\
	gtkrecentchooserdefault.h	\
	gtkrecentchooserutils.h		\
	gtkrecentchooserprivate.h	\
	gtksearchengine.h		\
	gtksearchenginebeagle.h		\
	gtksearchenginetracker.h	\
	gtksearchenginesimple.h		\
	gtksearchenginequartz.h		\
	gtksequence.h			\
	gtksocketprivate.h		\
	gtktextbtree.h			\
	gtktextbufferserializ.h		\
	gtktextchildprivate.h		\
	gtktextdisplay.h		\
	gtktextiterprivate.h		\
	gtktextlayout.h			\
	gtktextmarkprivate.h		\
	gtktextsegment.h		\
	gtktexttagprivate.h		\
	gtktexttypes.h			\
	gtktextutil.h			\
	gtkthemes.h			\
	gtktoggleactionprivate.h	\
	gtktrayicon.h			\
	gtktreedatalist.h		\
	gtktreeprivate.h		\
	gtktoolpaletteprivate.h		\
	gtktypebuiltins.h		\
	gtkxembed.h			\
	gtkwin32embed.h			\
	gtkwin32embedwidget.h		\
	gtkwindow-decorate.h		\
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

CPPFLAGS += \
	-UGTK_DISABLE_SINGLE_INCLUDES

GTKDOC_LIBS = \
	$(top_builddir)/gdk/$(gdktargetlib)        			\
	$(top_builddir)/gtk/$(gtktargetlib)


# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS=--sgml-mode --output-format=xml --name-space=gtk

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE)
content_files =					\
	version.xml				\
	running.sgml				\
	building.sgml				\
	changes-1.2.sgml			\
	changes-2.0.sgml			\
	compiling.sgml				\
	directfb.sgml				\
	drawing-model.xml			\
	glossary.xml				\
	migrating-checklist.sgml		\
        migrating-ClientSideWindows.sgml	\
	migrating-GtkAboutDialog.sgml		\
	migrating-GtkAction.sgml		\
	migrating-GtkAssistant.sgml		\
	migrating-GtkBuilder.sgml		\
	migrating-GtkColorButton.sgml		\
	migrating-GtkComboBox.sgml		\
	migrating-GtkEntry-icons.sgml		\
	migrating-GtkFileChooser.sgml		\
	migrating-GtkIconView.sgml		\
	migrating-GtkLabel-links.sgml		\
	migrating-GtkLinkButton.sgml		\
	migrating-GtkRecentChooser.sgml 	\
        migrating-GtkTooltip.sgml		\
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
	gtk-builder-convert.xml			\
	visual_index.xml

expand_content_files = 				\
	drawing-model.xml			\
	glossary.xml				\
	migrating-checklist.sgml		\
        migrating-ClientSideWindows.sgml	\
	migrating-GtkAction.sgml		\
	migrating-GtkComboBox.sgml		\
	migrating-GtkEntry-icons.sgml		\
	migrating-GtkFileChooser.sgml		\
	migrating-GtkIconView.sgml		\
	migrating-GtkAboutDialog.sgml		\
	migrating-GtkColorButton.sgml		\
	migrating-GtkAssistant.sgml		\
	migrating-GtkRecentChooser.sgml 	\
	migrating-GtkLabel-links.sgml		\
	migrating-GtkLinkButton.sgml		\
	migrating-GtkBuilder.sgml		\
	migrating-GtkTooltip.sgml		\
	tree_widget.sgml			\
	text_widget.sgml			\
	question_index.sgml

# Images to copy into HTML directory
HTML_IMAGES = \
	$(top_srcdir)/gtk/stock-icons/24/help-about.png			\
	$(top_srcdir)/gtk/stock-icons/24/list-add.png			\
	$(top_srcdir)/gtk/stock-icons/20/gtk-apply.png			\
	$(top_srcdir)/gtk/stock-icons/24/format-text-bold.png		\
	$(top_srcdir)/gtk/stock-icons/20/gtk-cancel.png			\
	$(top_srcdir)/gtk/stock-icons/24/gtk-caps-lock-warning.png	\
	$(top_srcdir)/gtk/stock-icons/24/media-optical.png		\
	$(top_srcdir)/gtk/stock-icons/24/edit-clear.png			\
	$(top_srcdir)/gtk/stock-icons/24/window-close.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-connect.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-convert.png		\
	$(top_srcdir)/gtk/stock-icons/24/edit-copy.png			\
	$(top_srcdir)/gtk/stock-icons/24/edit-cut.png			\
	$(top_srcdir)/gtk/stock-icons/24/edit-delete.png		\
	$(top_srcdir)/gtk/stock-icons/48/dialog-password.png		\
	$(top_srcdir)/gtk/stock-icons/48/dialog-error.png		\
	$(top_srcdir)/gtk/stock-icons/48/dialog-information.png		\
	$(top_srcdir)/gtk/stock-icons/48/dialog-question.png		\
	$(top_srcdir)/gtk/stock-icons/48/dialog-warning.png		\
	$(top_srcdir)/gtk/stock-icons/24/folder.png			\
	$(top_srcdir)/gtk/stock-icons/24/gtk-disconnect.png		\
	$(top_srcdir)/gtk/stock-icons/32/gtk-dnd.png			\
	$(top_srcdir)/gtk/stock-icons/32/gtk-dnd-multiple.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-edit.png			\
	$(top_srcdir)/gtk/stock-icons/24/system-run.png			\
	$(top_srcdir)/gtk/stock-icons/24/text-x-generic.png		\
	$(top_srcdir)/gtk/stock-icons/24/edit-find.png			\
	$(top_srcdir)/gtk/stock-icons/24/edit-find-replace.png		\
	$(top_srcdir)/gtk/stock-icons/24/go-bottom.png			\
	$(top_srcdir)/gtk/stock-icons/24/go-first-ltr.png		\
	$(top_srcdir)/gtk/stock-icons/24/go-first-rtl.png		\
	$(top_srcdir)/gtk/stock-icons/24/go-last-ltr.png		\
	$(top_srcdir)/gtk/stock-icons/24/go-last-rtl.png		\
	$(top_srcdir)/gtk/stock-icons/24/go-top.png			\
	$(top_srcdir)/gtk/stock-icons/24/go-previous-ltr.png		\
	$(top_srcdir)/gtk/stock-icons/24/go-previous-rtl.png		\
	$(top_srcdir)/gtk/stock-icons/24/go-down.png			\
	$(top_srcdir)/gtk/stock-icons/24/go-next-ltr.png		\
	$(top_srcdir)/gtk/stock-icons/24/go-next-rtl.png		\
	$(top_srcdir)/gtk/stock-icons/24/go-up.png			\
	$(top_srcdir)/gtk/stock-icons/24/drive-harddisk.png		\
	$(top_srcdir)/gtk/stock-icons/24/network-idle.png		\
	$(top_srcdir)/gtk/stock-icons/24/help-contents.png		\
	$(top_srcdir)/gtk/stock-icons/24/go-home.png			\
	$(top_srcdir)/gtk/stock-icons/24/gtk-index.png			\
	$(top_srcdir)/gtk/stock-icons/24/format-indent-more-ltr.png	\
	$(top_srcdir)/gtk/stock-icons/24/format-indent-more-rtl.png	\
	$(top_srcdir)/gtk/stock-icons/24/format-indent-less-ltr.png	\
	$(top_srcdir)/gtk/stock-icons/24/format-indent-less-rtl.png	\
	$(top_srcdir)/gtk/stock-icons/24/format-text-italic.png		\
	$(top_srcdir)/gtk/stock-icons/24/go-jump-ltr.png		\
	$(top_srcdir)/gtk/stock-icons/24/go-jump-rtl.png		\
	$(top_srcdir)/gtk/stock-icons/24/format-justify-center.png	\
	$(top_srcdir)/gtk/stock-icons/24/format-justify-fill.png	\
	$(top_srcdir)/gtk/stock-icons/24/format-justify-left.png	\
	$(top_srcdir)/gtk/stock-icons/24/format-justify-right.png	\
	$(top_srcdir)/gtk/stock-icons/24/image-missing.png		\
	$(top_srcdir)/gtk/stock-icons/24/document-new.png		\
	$(top_srcdir)/gtk/stock-icons/20/gtk-no.png			\
	$(top_srcdir)/gtk/stock-icons/20/gtk-ok.png			\
	$(top_srcdir)/gtk/stock-icons/24/document-open.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-orientation-landscape.png	\
	$(top_srcdir)/gtk/stock-icons/24/gtk-orientation-portrait.png	\
	$(top_srcdir)/gtk/stock-icons/24/gtk-orientation-reverse-landscape.png \
	$(top_srcdir)/gtk/stock-icons/24/gtk-orientation-reverse-portrait.png \
	$(top_srcdir)/gtk/stock-icons/24/edit-paste.png			\
	$(top_srcdir)/gtk/stock-icons/24/gtk-preferences.png		\
	$(top_srcdir)/gtk/stock-icons/24/document-print.png		\
	$(top_srcdir)/gtk/stock-icons/24/printer-error.png		\
	$(top_srcdir)/gtk/stock-icons/24/printer-paused.png		\
	$(top_srcdir)/gtk/stock-icons/24/document-print-preview.png	\
	$(top_srcdir)/gtk/stock-icons/24/printer-info.png		\
	$(top_srcdir)/gtk/stock-icons/24/printer-warning.png		\
	$(top_srcdir)/gtk/stock-icons/24/document-properties.png	\
	$(top_srcdir)/gtk/stock-icons/24/application-exit.png		\
	$(top_srcdir)/gtk/stock-icons/24/edit-redo-ltr.png		\
	$(top_srcdir)/gtk/stock-icons/24/edit-redo-rtl.png		\
	$(top_srcdir)/gtk/stock-icons/24/view-refresh.png		\
	$(top_srcdir)/gtk/stock-icons/24/list-remove.png		\
	$(top_srcdir)/gtk/stock-icons/24/document-revert-ltr.png	\
	$(top_srcdir)/gtk/stock-icons/24/document-revert-rtl.png	\
	$(top_srcdir)/gtk/stock-icons/24/media-floppy.png		\
	$(top_srcdir)/gtk/stock-icons/24/document-save.png		\
	$(top_srcdir)/gtk/stock-icons/24/document-save-as.png		\
	$(top_srcdir)/gtk/stock-icons/24/edit-select-all.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-select-color.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-color-picker.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-font.png			\
	$(top_srcdir)/gtk/stock-icons/24/view-sort-ascending.png	\
	$(top_srcdir)/gtk/stock-icons/24/view-sort-descending.png	\
	$(top_srcdir)/gtk/stock-icons/24/tools-check-spelling.png	\
	$(top_srcdir)/gtk/stock-icons/24/process-stop.png		\
	$(top_srcdir)/gtk/stock-icons/24/format-text-strikethrough.png	\
	$(top_srcdir)/gtk/stock-icons/24/gtk-undelete-ltr.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-undelete-rtl.png		\
	$(top_srcdir)/gtk/stock-icons/24/format-text-underline.png	\
	$(top_srcdir)/gtk/stock-icons/24/edit-undo-ltr.png		\
	$(top_srcdir)/gtk/stock-icons/24/edit-undo-rtl.png		\
	$(top_srcdir)/gtk/stock-icons/20/gtk-yes.png			\
	$(top_srcdir)/gtk/stock-icons/24/zoom-original.png		\
	$(top_srcdir)/gtk/stock-icons/24/zoom-fit-best.png		\
	$(top_srcdir)/gtk/stock-icons/24/zoom-in.png			\
	$(top_srcdir)/gtk/stock-icons/24/zoom-out.png			\
	$(top_srcdir)/gtk/stock-icons/24/media-seek-forward-ltr.png	\
	$(top_srcdir)/gtk/stock-icons/24/media-seek-forward-rtl.png	\
	$(top_srcdir)/gtk/stock-icons/24/media-skip-forward-ltr.png	\
	$(top_srcdir)/gtk/stock-icons/24/media-skip-forward-rtl.png	\
	$(top_srcdir)/gtk/stock-icons/24/media-playback-pause.png	\
	$(top_srcdir)/gtk/stock-icons/24/media-playback-start-ltr.png	\
	$(top_srcdir)/gtk/stock-icons/24/media-playback-start-rtl.png	\
	$(top_srcdir)/gtk/stock-icons/24/media-seek-backward-ltr.png	\
	$(top_srcdir)/gtk/stock-icons/24/media-seek-backward-rtl.png	\
	$(top_srcdir)/gtk/stock-icons/24/media-record.png		\
	$(top_srcdir)/gtk/stock-icons/24/media-skip-backward-ltr.png	\
	$(top_srcdir)/gtk/stock-icons/24/media-skip-backward-rtl.png	\
	$(top_srcdir)/gtk/stock-icons/24/media-playback-stop.png	\
	$(top_srcdir)/gtk/stock-icons/24/view-fullscreen.png		\
	$(top_srcdir)/gtk/stock-icons/24/view-restore.png		\
	$(top_srcdir)/gtk/stock-icons/24/gtk-page-setup.png		\
	$(srcdir)/images/aboutdialog.png				\
	$(srcdir)/images/accel-label.png				\
	$(srcdir)/images/assistant.png					\
	$(srcdir)/images/button.png					\
	$(srcdir)/images/check-button.png				\
	$(srcdir)/images/color-button.png				\
	$(srcdir)/images/colorsel.png					\
	$(srcdir)/images/combo-box.png					\
	$(srcdir)/images/combo-box-entry.png				\
	$(srcdir)/images/entry.png					\
	$(srcdir)/images/figure-hierarchical-drawing.png		\
	$(srcdir)/images/figure-windowed-label.png			\
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
	$(srcdir)/images/spinner.png					\
	$(srcdir)/images/statusbar.png					\
	$(srcdir)/images/toggle-button.png				\
	$(srcdir)/images/toolbar.png					\
	$(srcdir)/images/toolpalette.png				\
	$(srcdir)/images/tree-view-coordinates.png			\
	$(srcdir)/images/volumebutton.png				\
	$(srcdir)/images/window.png					\
	$(srcdir)/images/layout-btlr.png				\
	$(srcdir)/images/layout-btrl.png				\
	$(srcdir)/images/layout-lrbt.png				\
	$(srcdir)/images/layout-lrtb.png				\
	$(srcdir)/images/layout-rlbt.png				\
	$(srcdir)/images/layout-rltb.png				\
	$(srcdir)/images/layout-tblr.png				\
	$(srcdir)/images/layout-tbrl.png

# Extra options to supply to gtkdoc-fixref
FIXXREF_OPTIONS=--extra-dir=../gdk/html \
	--extra-dir=$(GLIB_PREFIX)/share/gtk-doc/html/gobject \
	--extra-dir=$(GLIB_PREFIX)/share/gtk-doc/html/glib \
	--extra-dir=$(ATK_PREFIX)/share/gtk-doc/html/atk \
	--extra-dir=$(PANGO_PREFIX)/share/gtk-doc/html/pango \
	--extra-dir=$(CAIRO_PREFIX)/share/gtk-doc/html/cairo

include $(top_srcdir)/gtk-doc.make

# Other files to distribute
EXTRA_DIST += version.xml.in

########################################################################

if ENABLE_MAN

man_MANS = gtk-query-immodules-2.0.1 gtk-update-icon-cache.1 gtk-builder-convert.1

%.1 : %.xml 
	@XSLTPROC@ -nonet http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $<

BUILT_EXTRA_DIST = $(man_MANS)

DISTCLEANFILES = $(man_MANS)

endif

dist-hook-local: $(BUILT_EXTRA_DIST)
	files='$(BUILT_EXTRA_DIST)';				\
	for f in $$files; do					\
	  if test -f $$f; then d=.; else d=$(srcdir); fi;	\
	  cp $$d/$$f $(distdir) || exit 1; done

-include $(top_srcdir)/git.mk
