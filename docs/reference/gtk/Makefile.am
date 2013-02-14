## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.6

# The name of the module.
DOC_MODULE=gtk3

# The top-level SGML file.
DOC_MAIN_SGML_FILE=gtk-docs.sgml

# Extra options to supply to gtkdoc-scan
SCAN_OPTIONS=--deprecated-guards="GTK_ENABLE_BROKEN|GTK_DISABLE_DEPRECATED"

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR=$(top_srcdir)/gtk

# Used for dependencies
HFILE_GLOB=$(top_srcdir)/gtk/*.h
CFILE_GLOB=$(top_srcdir)/gtk/*.c

# Header files to ignore when scanning
IGNORE_HFILES=				\
	fnmatch.h			\
	gactionmuxer.h			\
	gactionobserver.h		\
	gactionobservable.h		\
	gtk9slice.h			\
	gtkanimationdescription.h	\
	gtkdebug.h			\
	gtkactionhelper.h		\
	gtkapplicationprivate.h		\
	gtkbuilderprivate.h		\
	gtkcolorchooserprivate.h	\
	gtkcoloreditorprivate.h		\
	gtkcolorplaneprivate.h		\
	gtkcolorscaleprivate.h		\
	gtkcolorswatchprivate.h		\
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
	gtkmenuprivate.h		\
	gtkmnemonichash.h		\
	gtkmodelmenu.h			\
	gtkmodelmenuitem.h		\
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
	gtktimeline.h			\
	gtkthemes.h			\
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
AM_CPPFLAGS = \
	-I$(top_srcdir) 	\
	-I$(top_builddir) 	\
	-I$(top_builddir)/gdk	\
	-DGTK_ENABLE_BROKEN	\
	$(GTK_DEBUG_FLAGS)	\
	$(GTK_DEP_CFLAGS)

CPPFLAGS += \
	-UGTK_DISABLE_SINGLE_INCLUDES

GTKDOC_LIBS = \
	$(top_builddir)/gdk/libgdk-3.la \
	$(top_builddir)/gtk/libgtk-3.la	\
	$(GTK_DEP_LIBS)


# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS=--sgml-mode --output-format=xml --name-space=gtk

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE)
content_files =					\
	version.xml				\
	running.sgml				\
	building.sgml				\
	compiling.sgml				\
	drawing-model.xml			\
	glossary.xml				\
	migrating-2to3.xml			\
	migrating-checklist.sgml		\
	migrating-unique-GtkApplication.xml	\
	migrating-smclient-GtkApplication.xml	\
	migrating-GtkGrid.xml			\
	migrating-GtkStyleContext.xml		\
	objects_grouped.sgml			\
	osx.sgml				\
	broadway.xml				\
	wayland.xml				\
	question_index.sgml			\
	resources.sgml				\
	text_widget.sgml			\
	tree_widget.sgml			\
	windows.sgml				\
	x11.sgml				\
	gtk-query-immodules-3.0.xml		\
	gtk-update-icon-cache.xml		\
	gtk-launch.xml				\
	broadwayd.xml				\
	visual_index.xml			\
	getting_started.xml			\
	overview.xml

expand_content_files = 				\
	compiling.sgml				\
	drawing-model.xml			\
	getting_started.xml			\
	glossary.xml				\
	migrating-2to3.xml			\
	migrating-checklist.sgml		\
	migrating-unique-GtkApplication.xml	\
	migrating-smclient-GtkApplication.xml	\
	migrating-GtkGrid.xml			\
	migrating-GtkStyleContext.xml		\
	question_index.sgml			\
	text_widget.sgml			\
	tree_widget.sgml

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
	$(srcdir)/images/appchooserbutton.png				\
	$(srcdir)/images/appchooserdialog.png				\
	$(srcdir)/images/assistant.png					\
	$(srcdir)/images/box-packing.png				\
	$(srcdir)/images/box-expand.png				\
	$(srcdir)/images/button.png					\
	$(srcdir)/images/check-button.png				\
	$(srcdir)/images/color-button.png				\
	$(srcdir)/images/colorsel.png					\
	$(srcdir)/images/colorchooser.png				\
	$(srcdir)/images/combo-box.png					\
	$(srcdir)/images/combo-box-entry.png				\
	$(srcdir)/images/entry.png					\
	$(srcdir)/images/search-entry.png				\
	$(srcdir)/images/figure-hierarchical-drawing.png		\
	$(srcdir)/images/figure-windowed-label.png			\
	$(srcdir)/images/file-button.png				\
	$(srcdir)/images/filechooser.png				\
	$(srcdir)/images/font-button.png				\
	$(srcdir)/images/fontsel.png					\
	$(srcdir)/images/fontchooser.png				\
	$(srcdir)/images/frame.png					\
	$(srcdir)/images/icon-view.png					\
	$(srcdir)/images/image.png					\
	$(srcdir)/images/label.png					\
	$(srcdir)/images/levelbar.png					\
	$(srcdir)/images/link-button.png				\
	$(srcdir)/images/list-and-tree.png				\
	$(srcdir)/images/lock-button.png				\
	$(srcdir)/images/lockbutton.png	        			\
	$(srcdir)/images/lockbutton-locked.png				\
	$(srcdir)/images/lockbutton-unlocked.png			\
	$(srcdir)/images/lockbutton-sorry.png				\
	$(srcdir)/images/menubar.png					\
	$(srcdir)/images/menu-button.png				\
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
	$(srcdir)/images/layout-tbrl.png				\
	$(srcdir)/images/widget-hvalign.png				\
	$(srcdir)/images/window-default.png				\
	$(srcdir)/images/hello-world.png				\
	$(srcdir)/images/grid-packing.png				\
	$(srcdir)/images/drawing.png					\
	$(srcdir)/images/switch.png					\
	$(srcdir)/images/linear.png					\
	$(srcdir)/images/ease.png					\
	$(srcdir)/images/ease-in-out.png				\
	$(srcdir)/images/ease-in.png					\
	$(srcdir)/images/ease-out.png					\
	$(srcdir)/images/gradient1.png					\
	$(srcdir)/images/gradient2.png					\
	$(srcdir)/images/gradient3.png					\
	$(srcdir)/images/gradient4.png					\
	$(srcdir)/images/border1.png					\
	$(srcdir)/images/border2.png					\
	$(srcdir)/images/border3.png					\
	$(srcdir)/images/slices.png					\
	$(srcdir)/images/checks.png					\
	$(srcdir)/images/options.png					\
	$(srcdir)/images/arrows.png					\
	$(srcdir)/images/expanders.png					\
	$(srcdir)/images/background.png					\
	$(srcdir)/images/frames.png					\
	$(srcdir)/images/frame-gap.png					\
	$(srcdir)/images/sliders.png					\
	$(srcdir)/images/focus.png					\
	$(srcdir)/images/handles.png					\
	$(srcdir)/images/extensions.png					\
	$(srcdir)/images/numerableicon.png				\
	$(srcdir)/images/numerableicon2.png				\
	$(srcdir)/images/bloatpad-osx.png				\
	$(srcdir)/images/bloatpad-gnome.png				\
	$(srcdir)/images/bloatpad-xfce.png				\
	$(srcdir)/images/down-center.png				\
	$(srcdir)/images/down-end.png					\
	$(srcdir)/images/down-start.png					\
	$(srcdir)/images/left-center.png				\
	$(srcdir)/images/left-end.png					\
	$(srcdir)/images/left-start.png					\
	$(srcdir)/images/right-center.png				\
	$(srcdir)/images/right-end.png					\
	$(srcdir)/images/right-start.png				\
	$(srcdir)/images/up-center.png					\
	$(srcdir)/images/up-end.png					\
	$(srcdir)/images/up-start.png

# Extra options to supply to gtkdoc-fixref
FIXXREF_OPTIONS=--extra-dir=../gdk/html \
	--extra-dir=$(GLIB_PREFIX)/share/gtk-doc/html/gobject \
	--extra-dir=$(GLIB_PREFIX)/share/gtk-doc/html/glib \
	--extra-dir=$(GLIB_PREFIX)/share/gtk-doc/html/gio \
	--extra-dir=$(ATK_PREFIX)/share/gtk-doc/html/atk \
	--extra-dir=$(PANGO_PREFIX)/share/gtk-doc/html/pango \
	--extra-dir=$(CAIRO_PREFIX)/share/gtk-doc/html/cairo

include $(top_srcdir)/gtk-doc.make

# Other files to distribute
EXTRA_DIST += version.xml.in gtk3.types.in

########################################################################

man_MANS = 				\
	gtk-query-immodules-3.0.1	\
	gtk-update-icon-cache.1		\
	gtk-launch.1			\
	broadwayd.1

if ENABLE_MAN

XSLTPROC_FLAGS = \
        --nonet \
        --stringparam man.output.quietly 1 \
        --stringparam funcsynopsis.style ansi \
        --stringparam man.th.extra1.suppress 1 \
        --stringparam man.authors.section.enabled 0 \
        --stringparam man.copyright.section.enabled 0

.xml.1:
	$(AM_V_GEN) $(XSLTPROC) $(XSLTPROC_FLAGS) http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $<

dist-local-check-mans-enabled:
	if grep "Man generation disabled" $(man_MANS) >/dev/null; then $(RM) $(man_MANS); fi

else

$(man_MANS):
	echo Man generation disabled.  Creating dummy $@.  Configure with --enable-man to enable it.
	echo Man generation disabled.  Remove this file, configure with --enable-man, and rebuild > $@

dist-local-check-mans-enabled:
	echo "*** --enable-man must be used in order to make dist"
	false

endif

MAINTAINERCLEANFILES = $(man_MANS) $(BUILT_SOURCES)

EXTRA_DIST += $(man_MANS)

if ENABLE_GTK_DOC
TESTS_ENVIRONMENT = cd $(srcdir) && \
  DOC_MODULE=$(DOC_MODULE) DOC_MAIN_SGML_FILE=$(DOC_MAIN_SGML_FILE) \
  SRCDIR=$(abs_srcdir) BUILDDIR=$(abs_builddir)
#TESTS = $(GTKDOC_CHECK)
endif

dist-hook-local: dist-local-check-mans-enabled all-local

gtk-docs-clean: clean
	cd $(srcdir) && rm -rf xml html

-include $(top_srcdir)/git.mk
