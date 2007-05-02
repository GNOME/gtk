# Makefile.am for gtk+/gtk

SUBDIRS=theme-bits

if OS_UNIX
SUBDIRS += xdgmime
GTK_PRINT_PREVIEW_COMMAND="evince --preview %f"
endif

DIST_SUBDIRS=theme-bits xdgmime

if HAVE_CUPS
GTK_PRINT_BACKENDS=file,cups
else
GTK_PRINT_BACKENDS=file,lpr
endif

INCLUDES =						\
	-DG_LOG_DOMAIN=\"Gtk\"				\
	-DGTK_LIBDIR=\"$(libdir)\"			\
	-DGTK_DATADIR=\"$(datadir)\"			\
	-DGTK_DATA_PREFIX=\"$(prefix)\"			\
	-DGTK_SYSCONFDIR=\"$(sysconfdir)\"		\
	-DGTK_VERSION=\"$(GTK_VERSION)\"		\
	-DGTK_BINARY_VERSION=\"$(GTK_BINARY_VERSION)\"	\
	-DGTK_HOST=\"$(host)\"				\
	-DGTK_COMPILATION				\
	-DGTK_PRINT_BACKENDS=\"$(GTK_PRINT_BACKENDS)\"	\
	-DGTK_PRINT_PREVIEW_COMMAND=\"$(GTK_PRINT_PREVIEW_COMMAND)\"	\
	-I$(top_builddir)/gtk				\
	-I$(top_srcdir) -I../gdk			\
	-I$(top_srcdir)/gdk				\
	-I$(top_srcdir)/gdk-pixbuf -I../gdk-pixbuf	\
	-DGDK_PIXBUF_DISABLE_DEPRECATED			\
	-DGDK_DISABLE_DEPRECATED			\
	-DGTK_FILE_SYSTEM_ENABLE_UNSUPPORTED		\
	-DGTK_PRINT_BACKEND_ENABLE_UNSUPPORTED		\
	$(GTK_DEBUG_FLAGS)				\
	$(GTK_DEP_CFLAGS)				\
	$(gtk_clipboard_dnd_c_sources_CFLAGS)

gtarget=$(gdktarget)

if PLATFORM_WIN32
no_undefined = -no-undefined
endif

if OS_WIN32
gtk_def = gtk.def
gtk_win32_symbols = -export-symbols gtk.def

gtk_win32_res = gtk-win32-res.o
gtk_win32_res_ldflag = -Wl,gtk-win32-res.o

gtk-win32-res.o : gtk-win32.rc
	$(WINDRES) gtk-win32.rc $@

install-def-file: gtk.def
	$(INSTALL) gtk.def $(DESTDIR)$(libdir)/gtk-win32-2.0.def
uninstall-def-file:
	-rm $(DESTDIR)$(libdir)/gtk-win32-2.0.def
else
install-def-file:
uninstall-def-file:
endif

if MS_LIB_AVAILABLE
noinst_DATA = gtk-win32-$(GTK_API_VERSION).lib

gtk-win32-$(GTK_API_VERSION).lib: libgtk-win32-$(GTK_API_VERSION).la gtk.def
	lib -name:libgtk-win32-$(GTK_API_VERSION)-@LT_CURRENT_MINUS_AGE@.dll -def:gtk.def -out:$@

install-ms-lib:
	$(INSTALL) gtk-win32-$(GTK_API_VERSION).lib $(DESTDIR)$(libdir)

uninstall-ms-lib:
	-rm $(DESTDIR)$(libdir)/gtk-win32-$(GTK_API_VERSION).lib
else
install-ms-lib:
uninstall-ms-lib:
endif

gtk.def: gtk.symbols
	(echo -e EXPORTS; $(CPP) -P -DINCLUDE_VARIABLES -DG_OS_WIN32 -DALL_FILES - <$(srcdir)/gtk.symbols | sed -e '/^$$/d' -e 's/^/	/' -e 's/G_GNUC_[^ ]*//g') > gtk.def

gtkalias.h: gtk.symbols
	  $(PERL) $(srcdir)/makegtkalias.pl < $(srcdir)/gtk.symbols > gtkalias.h
gtkaliasdef.c: gtk.symbols
	  $(PERL) $(srcdir)/makegtkalias.pl -def < $(srcdir)/gtk.symbols > gtkaliasdef.c

check-aliases:
	@$(TESTS_ENVIRONMENT) $(srcdir)/aliasfilescheck.sh

.PHONY: check-aliases

TESTS_ENVIRONMENT = srcdir="$(srcdir)" gtk_all_c_sources="$(gtk_all_c_sources)"
TESTS = aliasfilescheck.sh
if OS_LINUX
TESTS += abicheck.sh pltcheck.sh
endif

# libtool stuff: set version and export symbols for resolving
# since automake doesn't support conditionalized libsomething_la_LDFLAGS
# we use the general approach here
libgtkincludedir = $(includedir)/gtk-2.0/gtk
libadd =								\
	$(top_builddir)/gdk-pixbuf/libgdk_pixbuf-$(GTK_API_VERSION).la	\
	$(top_builddir)/gdk/$(gdktargetlib)				\
	$(GTK_DEP_LIBS)

if OS_UNIX
libadd += xdgmime/libxdgmime.la
endif

# common options for the various packages.
libtool_opts =							\
  -version-info $(LT_VERSION_INFO)				\
  -export-dynamic $(no_undefined) $(LIBTOOL_EXPORT_OPTIONS)	\
  -rpath $(libdir) $(libgtk_target_ldflags)


#
# setup source file variables
#

# GTK+ header files for public installation (non-generated, or generated
# by configure)
gtk_public_h_sources =          \
	gtk.h			\
	gtkaboutdialog.h	\
	gtkaccelgroup.h		\
	gtkaccellabel.h		\
	gtkaccelmap.h		\
	gtkaccessible.h		\
	gtkaction.h		\
	gtkactiongroup.h	\
	gtkadjustment.h		\
	gtkalignment.h		\
	gtkarrow.h		\
	gtkaspectframe.h	\
	gtkassistant.h		\
	gtkbbox.h		\
	gtkbin.h		\
	gtkbindings.h		\
	gtkbox.h		\
	gtkbutton.h		\
	gtkcalendar.h		\
	gtkcelleditable.h	\
	gtkcelllayout.h		\
	gtkcellrenderer.h	\
	gtkcellrendereraccel.h	\
	gtkcellrenderercombo.h	\
	gtkcellrendererpixbuf.h	\
	gtkcellrendererprogress.h \
	gtkcellrendererspin.h   \
	gtkcellrenderertext.h	\
	gtkcellrenderertoggle.h	\
	gtkcellview.h		\
	gtkcheckbutton.h	\
	gtkcheckmenuitem.h	\
	gtkclipboard.h		\
	gtkclist.h		\
	gtkcolorbutton.h	\
	gtkcolorsel.h		\
	gtkcolorseldialog.h	\
	gtkcombo.h		\
	gtkcombobox.h		\
	gtkcomboboxentry.h	\
	gtkcontainer.h		\
	gtkctree.h		\
	gtkcurve.h		\
	gtkdebug.h              \
	gtkdialog.h		\
	gtkdnd.h		\
	gtkdrawingarea.h	\
	gtkeditable.h           \
	gtkentry.h		\
	gtkentrycompletion.h	\
	gtkenums.h		\
	gtkeventbox.h		\
	gtkexpander.h		\
	gtkfilechooser.h        \
	gtkfilechooserbutton.h  \
	gtkfilechooserdialog.h  \
	gtkfilechooserwidget.h  \
	gtkfilefilter.h		\
	gtkfilesel.h		\
	gtkfixed.h		\
	gtkfontbutton.h		\
	gtkfontsel.h		\
	gtkframe.h		\
	gtkgamma.h		\
	gtkgc.h			\
	gtkhandlebox.h		\
	gtkhbbox.h		\
	gtkhbox.h		\
	gtkhpaned.h		\
	gtkhruler.h		\
	gtkhscale.h		\
	gtkhscrollbar.h		\
	gtkhseparator.h		\
	gtkiconfactory.h	\
	gtkicontheme.h		\
	gtkiconview.h		\
	gtkimage.h		\
	gtkimagemenuitem.h	\
	gtkimcontext.h		\
	gtkimcontextsimple.h	\
	gtkimmodule.h		\
	gtkimmulticontext.h	\
	gtkinputdialog.h	\
	gtkinvisible.h		\
	gtkitem.h		\
	gtkitemfactory.h	\
	gtklabel.h		\
	gtklayout.h             \
	gtklinkbutton.h		\
	gtklist.h		\
	gtklistitem.h		\
	gtkliststore.h		\
	gtkmain.h		\
	gtkmenu.h		\
	gtkmenubar.h		\
	gtkmenuitem.h		\
	gtkmenushell.h		\
	gtkmenutoolbutton.h	\
	gtkmessagedialog.h	\
	gtkmisc.h		\
	gtkmodules.h		\
	gtknotebook.h		\
	gtkobject.h		\
	gtkoldeditable.h	\
	gtkoptionmenu.h		\
	gtkpagesetup.h		\
	gtkpaned.h		\
	gtkpapersize.h		\
	gtkpixmap.h		\
	gtkplug.h		\
	gtkpreview.h		\
	gtkprintcontext.h	\
	gtkprintoperation.h	\
	gtkprintoperationpreview.h	\
	gtkprintsettings.h	\
	gtkprivate.h		\
	gtkprogress.h		\
	gtkprogressbar.h	\
	gtkradioaction.h	\
	gtkradiobutton.h	\
	gtkradiomenuitem.h	\
	gtkradiotoolbutton.h	\
	gtkrange.h		\
	gtkrc.h			\
	gtkrecentchooser.h 	\
	gtkrecentchooserdialog.h \
	gtkrecentchoosermenu.h 	\
	gtkrecentchooserwidget.h \
	gtkrecentfilter.h 	\
	gtkrecentmanager.h 	\
	gtkruler.h		\
	gtkscale.h		\
	gtkscrollbar.h		\
	gtkscrolledwindow.h	\
	gtkselection.h		\
	gtkseparator.h		\
	gtkseparatormenuitem.h	\
	gtkseparatortoolitem.h	\
	gtksettings.h		\
	gtksignal.h		\
	gtksizegroup.h		\
	gtksocket.h		\
	gtkspinbutton.h		\
	gtkstatusbar.h		\
	gtkstatusicon.h		\
	gtkstock.h		\
	gtkstyle.h		\
	gtktable.h		\
	gtktearoffmenuitem.h    \
	gtktext.h		\
	gtktextbuffer.h		\
	gtktextbufferrichtext.h	\
	gtktextchild.h		\
	gtktextdisplay.h	\
	gtktextiter.h		\
	gtktextmark.h		\
	gtktexttag.h		\
	gtktexttagtable.h	\
	gtktextview.h		\
	gtktipsquery.h		\
	gtktoggleaction.h	\
	gtktogglebutton.h	\
	gtktoggletoolbutton.h	\
	gtktoolbar.h		\
	gtktoolbutton.h		\
	gtktoolitem.h		\
	gtktooltips.h		\
	gtktree.h		\
	gtktreednd.h		\
	gtktreeitem.h		\
	gtktreemodel.h		\
	gtktreemodelfilter.h	\
	gtktreemodelsort.h	\
	gtktreeselection.h	\
	gtktreesortable.h	\
	gtktreestore.h		\
	gtktreeview.h		\
	gtktreeviewcolumn.h	\
	gtktypeutils.h		\
	gtkuimanager.h		\
	gtkvbbox.h		\
	gtkvbox.h		\
	gtkviewport.h		\
	gtkvpaned.h		\
	gtkvruler.h		\
	gtkvscale.h		\
	gtkvscrollbar.h		\
	gtkvseparator.h		\
	gtkwidget.h		\
	gtkwindow.h

if OS_UNIX
gtk_unix_print_public_h_sources =    \
	gtkpagesetupunixdialog.h	\
	gtkprintunixdialog.h		\
	gtkprinter.h			\
	gtkprintjob.h
endif

# Installed header files without compatibility guarantees
# that are not included in gtk/gtk.h
gtk_semi_private_h_sources =    \
	gtktextlayout.h		\
	gtkfilesystem.h

# GTK+ header files that don't get installed
gtk_private_h_sources =		\
	gtkdndcursors.h		\
	gtkentryprivate.h	\
	gtkfilechooserdefault.h	\
	gtkfilechooserembed.h	\
	gtkfilechooserentry.h	\
	gtkfilechooserprivate.h	\
	gtkfilechoosersettings.h \
	gtkfilechooserutils.h	\
	gtkfilesystemmodel.h	\
	gtkfilesystemunix.h	\
	gtkhsv.h		\
	gtkiconcache.h		\
	gtkiconcachevalidator.h	\
	gtkintl.h		\
	gtkkeyhash.h		\
	gtkmnemonichash.h	\
	gtkpathbar.h		\
	gtkplugprivate.h	\
	gtkprintoperation-private.h\
	gtkprintutils.h		\
	gtkrbtree.h		\
	gtkrecentchooserdefault.h \
	gtkrecentchooserprivate.h \
	gtkrecentchooserutils.h \
	gtksequence.h		\
	gtksocketprivate.h	\
	gtktextbtree.h		\
	gtktextbufferserialize.h\
	gtktextchildprivate.h	\
	gtktextiterprivate.h	\
	gtktextmarkprivate.h	\
	gtktextsegment.h	\
	gtktexttagprivate.h	\
	gtktexttypes.h		\
	gtktextutil.h		\
	gtkthemes.h		\
	gtktoggleactionprivate.h\
	gtktreedatalist.h	\
	gtktreeprivate.h	\
	gtkwindow-decorate.h

# GTK+ C sources to build the library from
gtk_base_c_sources =            \
	fnmatch.c		\
	gtkaboutdialog.c	\
	gtkaccelgroup.c		\
	gtkaccellabel.c		\
	gtkaccelmap.c		\
	gtkaccessible.c		\
	gtkaction.c		\
	gtkactiongroup.c	\
	gtkadjustment.c		\
	gtkalignment.c		\
	gtkarrow.c		\
	gtkaspectframe.c	\
	gtkassistant.c		\
	gtkbbox.c		\
	gtkbin.c		\
	gtkbindings.c		\
	gtkbox.c		\
	gtkbutton.c		\
	gtkcalendar.c		\
	gtkcelleditable.c	\
	gtkcelllayout.c		\
	gtkcellrenderer.c	\
	gtkcellrendereraccel.c	\
	gtkcellrenderercombo.c	\
	gtkcellrendererpixbuf.c	\
	gtkcellrendererprogress.c \
	gtkcellrendererspin.c   \
	gtkcellrenderertext.c	\
	gtkcellrenderertoggle.c	\
	gtkcellview.c		\
	gtkcheckbutton.c	\
	gtkcheckmenuitem.c	\
	gtkclist.c		\
	gtkcolorbutton.c	\
	gtkcolorsel.c		\
	gtkcolorseldialog.c	\
	gtkcombo.c		\
	gtkcombobox.c		\
	gtkcomboboxentry.c	\
	gtkcontainer.c		\
	gtkctree.c		\
	gtkcurve.c		\
	gtkdialog.c		\
	gtkdrawingarea.c	\
	gtkeditable.c           \
	gtkentry.c		\
	gtkentrycompletion.c	\
	gtkeventbox.c		\
	gtkexpander.c		\
	gtkfilechooser.c	\
	gtkfilechooserbutton.c	\
	gtkfilechooserdefault.c	\
	gtkfilechooserdialog.c	\
	gtkfilechooserembed.c	\
	gtkfilechooserentry.c	\
	gtkfilechoosersettings.c \
	gtkfilechooserutils.c	\
	gtkfilechooserwidget.c	\
	gtkfilefilter.c		\
	gtkfilesel.c		\
	gtkfilesystem.c		\
	gtkfilesystemmodel.c	\
	gtkfixed.c		\
	gtkfontbutton.c         \
	gtkfontsel.c            \
	gtkframe.c		\
	gtkgamma.c		\
	gtkgc.c			\
	gtkhandlebox.c		\
	gtkhbbox.c		\
	gtkhbox.c		\
	gtkhpaned.c		\
	gtkhruler.c		\
	gtkhscale.c		\
	gtkhscrollbar.c		\
	gtkhseparator.c		\
	gtkhsv.c		\
	gtkiconcache.c		\
	gtkiconcachevalidator.c	\
	gtkiconfactory.c	\
	gtkicontheme.c		\
	gtkiconview.c		\
	gtkimage.c		\
	gtkimagemenuitem.c	\
	gtkimcontext.c		\
	gtkimcontextsimple.c	\
	gtkimmodule.c		\
	gtkimmulticontext.c	\
	gtkinputdialog.c	\
	gtkinvisible.c		\
	gtkitem.c		\
	gtkitemfactory.c	\
	gtkkeyhash.c		\
	gtklabel.c		\
	gtklayout.c		\
	gtklinkbutton.c		\
	gtklist.c		\
	gtklistitem.c		\
	gtkliststore.c		\
	gtkmain.c		\
	gtkmarshal.c		\
	gtkmarshalers.c		\
	gtkmenu.c		\
	gtkmenubar.c		\
	gtkmenuitem.c		\
	gtkmenushell.c		\
	gtkmenutoolbutton.c	\
	gtkmessagedialog.c	\
	gtkmisc.c		\
	gtkmnemonichash.c	\
	gtkmodules.c		\
	gtknotebook.c		\
	gtkobject.c		\
	gtkoldeditable.c	\
	gtkoptionmenu.c		\
	gtkpagesetup.c		\
	gtkpaned.c		\
	gtkpapersize.c		\
	gtkpathbar.c		\
	gtkpixmap.c		\
	gtkplug.c		\
	gtkpreview.c		\
	gtkprintcontext.c	\
	gtkprintoperation.c	\
	gtkprintoperationpreview.c	\
	gtkprintsettings.c	\
	gtkprintutils.c		\
	gtkprogress.c		\
	gtkprogressbar.c	\
	gtkradioaction.c	\
	gtkradiobutton.c	\
	gtkradiomenuitem.c	\
	gtkradiotoolbutton.c	\
	gtkrange.c		\
	gtkrbtree.c 		\
	gtkrc.c			\
	gtkrecentchooserdefault.c \
	gtkrecentchooserdialog.c \
	gtkrecentchoosermenu.c 	\
	gtkrecentchooserwidget.c \
	gtkrecentchooserutils.c \
	gtkrecentchooser.c 	\
	gtkrecentfilter.c 	\
	gtkrecentmanager.c 	\
	gtkruler.c		\
	gtkscale.c		\
	gtkscrollbar.c		\
	gtkscrolledwindow.c	\
	gtkselection.c		\
	gtkseparator.c		\
	gtkseparatormenuitem.c	\
	gtkseparatortoolitem.c	\
	gtksequence.c		\
	gtksettings.c		\
	gtksignal.c		\
	gtksizegroup.c		\
	gtksocket.c		\
	gtkspinbutton.c		\
	gtkstatusbar.c		\
	gtkstatusicon.c		\
	gtkstock.c		\
	gtkstyle.c		\
	gtktable.c		\
	gtktearoffmenuitem.c    \
	gtktext.c		\
	gtktextbtree.c		\
	gtktextbuffer.c		\
	gtktextbufferrichtext.c	\
	gtktextbufferserialize.c\
	gtktextchild.c		\
	gtktextdisplay.c	\
	gtktextiter.c		\
	gtktextlayout.c		\
	gtktextmark.c		\
	gtktextsegment.c	\
	gtktexttag.c		\
	gtktexttagtable.c	\
	gtktexttypes.c		\
	gtktextutil.c		\
	gtktextview.c		\
	gtkthemes.c		\
	gtktipsquery.c		\
	gtktoggleaction.c	\
	gtktogglebutton.c	\
	gtktoggletoolbutton.c	\
	gtktoolbar.c		\
	gtktoolbutton.c		\
	gtktoolitem.c		\
	gtktooltips.c		\
	gtktree.c		\
	gtktreedatalist.c	\
	gtktreednd.c		\
	gtktreeitem.c		\
	gtktreemodel.c		\
	gtktreemodelfilter.c	\
	gtktreemodelsort.c	\
	gtktreeselection.c	\
	gtktreesortable.c	\
	gtktreestore.c		\
	gtktreeview.c		\
	gtktreeviewcolumn.c	\
	gtktypebuiltins.c	\
	gtktypeutils.c		\
	gtkuimanager.c		\
	gtkvbbox.c		\
	gtkvbox.c		\
	gtkviewport.c		\
	gtkvpaned.c		\
	gtkvruler.c		\
	gtkvscale.c		\
	gtkvscrollbar.c		\
	gtkvseparator.c		\
	gtkwidget.c		\
	gtkwindow-decorate.c    \
	gtkwindow.c		\
	$(gtk_clipboard_dnd_c_sources)
gtk_c_sources = $(gtk_base_c_sources)
gtk_all_c_sources = $(gtk_base_c_sources)

gtk_os_unix_c_sources = \
	gtkfilesystemunix.c		\
	gtkpagesetupunixdialog.c	\
	gtkprinter.c			\
	gtkprinteroption.c		\
	gtkprinteroptionset.c		\
	gtkprinteroptionwidget.c	\
	gtkprintjob.c			\
	gtkprintoperation-unix.c	\
	gtkprintunixdialog.c		\
        gtkprintbackend.c
gtk_all_c_sources += $(gtk_os_unix_c_sources)
if OS_UNIX
gtk_private_h_sources += \
	gtkfilesystemunix.h		\
	gtkprintbackend.h		\
	gtkprinter-private.h		\
	gtkprinteroption.h		\
	gtkprinteroptionset.h		\
	gtkprinteroptionwidget.h
gtk_c_sources += $(gtk_os_unix_c_sources)
endif

gtk_os_win32_c_sources = \
	gtkfilesystemwin32.c		\
	gtkprint-win32.c		\
	gtkprintoperation-win32.c
gtk_all_c_sources += $(gtk_os_win32_c_sources)
if OS_WIN32
gtk_private_h_sources += gtkfilesystemwin32.h gtkprint-win32.h
gtk_c_sources += $(gtk_os_win32_c_sources)
endif

gtk_use_x11_c_sources = \
	gtkplug-x11.c   \
	gtksocket-x11.c \
	gtkxembed.c     \
	gtktrayicon-x11.c
gtk_use_win32_c_sources = \
	gtkplug-win32.c   \
	gtksocket-win32.c \
	gtkwin32embed.c   \
	gtkwin32embedwidget.c
gtk_use_stub_c_sources = \
	gtkplug-stub.c   \
	gtksocket-stub.c
gtk_all_c_sources += $(gtk_use_x11_c_sources) $(gtk_use_win32_c_sources) $(gtk_use_stub_c_sources)
if USE_X11
gtk_private_h_sources += gtkxembed.h gtktrayicon.h xembed.h
gtk_c_sources += $(gtk_use_x11_c_sources)
else
if USE_WIN32
gtk_private_h_sources += gtkwin32embed.h gtkwin32embedwidget.h
gtk_c_sources += $(gtk_use_win32_c_sources)
else
gtk_c_sources += $(gtk_use_stub_c_sources)
endif
endif

if USE_QUARTZ
gtk_clipboard_dnd_c_sources = gtkclipboard-quartz.c gtkdnd-quartz.c gtkquartz.c gtkquartz.h
gtk_clipboard_dnd_c_sources_CFLAGS = "-xobjective-c"
else
gtk_clipboard_dnd_c_sources = gtkclipboard.c gtkdnd.c
gtk_clipboard_dnd_c_sources_CFLAGS = 
endif

# we use our own built_sources variable rules to avoid automake's
# BUILT_SOURCES oddities
# we generate frequently rebuild files piggyback on a stamp file, so sources
# depending on them only get rebuild when the built source actually changed
# content

# built sources that get installed with the header files
gtk_built_public_sources =			\
	gtkmarshal.h				\
	gtktypebuiltins.h

# built headers that don't get installed
gtk_built_private_headers =			\
	gtkalias.h				\
	gtkmarshalers.h				\
	gtkbuiltincache.h

gtk_built_sources =				\
	gtkaliasdef.c				\
	gtktypebuiltins.c			\
	gtkmarshalers.c				\
	gtkmarshal.c				\
	${gtk_built_private_headers}		\
	${gtk_built_public_sources}

stamp_files =					\
	stamp-gtkmarshalers.h			\
	stamp-gtkmarshal.h			\
	stamp-gtktypebuiltins.h			\
	stamp-icons

# non-header sources (headers should be specified in the above variables)
# that don't serve as direct make target sources, i.e. they don't have
# their own .lo rules and don't get publically installed
gtk_extra_sources =				\
	paper_names.c				\
	paper_names_offsets.c			\
	gen-paper-names.c			\
	gtk.symbols				\
	gtkversion.h.in				\
	gtkmarshalers.list			\
	gtkmarshal.list

#
# setup GTK+ sources and their dependencies
#
MAINTAINERCLEANFILES = $(gtk_built_sources) $(stamp_files)
EXTRA_HEADERS =
EXTRA_DIST = $(gtk_private_h_sources) $(gtk_extra_sources)
EXTRA_DIST += $(gtk_built_sources)
EXTRA_DIST += $(STOCK_ICONS)

#
# rules to generate built sources
#
# setup autogeneration dependencies
gen_sources = xgen-gdef xgen-gtbh xgen-gtic xgen-gmh xgen-gmc xgen-gmlh xgen-gmlc
CLEANFILES = $(gen_sources)

BUILT_SOURCES = $(gtk_built_sources)

# all autogenerated files need to be generated in the srcdir,
# so old versions get remade and are not confused with newer
# versions in the build dir. thus a development setup requires
# srcdir to be writable, passing --disable-rebuilds to
# ../configure will supress all autogeneration rules.
gtkmarshalers.h: stamp-gtkmarshalers.h
	@true
stamp-gtkmarshalers.h: @REBUILD@ gtkmarshalers.list
	$(GLIB_GENMARSHAL) --prefix=_gtk_marshal $(srcdir)/gtkmarshalers.list --header >> xgen-gmlh \
	&& (cmp -s xgen-gmlh gtkmarshalers.h || cp xgen-gmlh gtkmarshalers.h) \
	&& rm -f xgen-gmlh \
	&& echo timestamp > $(@F)	
gtkmarshalers.c: @REBUILD@ gtkmarshalers.list
	(echo "#include \"gtkmarshalers.h\""; \
	 echo "#include \"gtkalias.h\""; \
	 $(GLIB_GENMARSHAL) --prefix=_gtk_marshal $(srcdir)/gtkmarshalers.list --body) >> xgen-gmlc \
	&& cp xgen-gmlc gtkmarshalers.c \
	&& rm -f xgen-gmlc

gtkmarshal.h: stamp-gtkmarshal.h
	@true
stamp-gtkmarshal.h: @REBUILD@ gtkmarshal.list
	echo "#ifndef GTK_DISABLE_DEPRECATED" > xgen-gmh					\
        && $(GLIB_GENMARSHAL) --prefix=gtk_marshal $(srcdir)/gtkmarshal.list --header >> xgen-gmh \
	&& echo "#endif /* GTK_DISABLE_DEPRECATED */" >> xgen-gmh \
	&& (cmp -s xgen-gmh gtkmarshal.h || cp xgen-gmh gtkmarshal.h) \
	&& rm -f xgen-gmh \
	&& echo timestamp > $(@F)
gtkmarshal.c: @REBUILD@ gtkmarshal.list
	(echo "#include \"gtkmarshal.h\""; \
	 echo "#include \"gtkalias.h\""; \
	 $(GLIB_GENMARSHAL) --prefix=gtk_marshal $(srcdir)/gtkmarshal.list --body; \
	 echo "#define __gtk_marshal_MARSHAL_C__"; \
	 echo "#include \"gtkaliasdef.c\"") >> xgen-gmc \
	&& cp xgen-gmc gtkmarshal.c \
	&& rm -f xgen-gmc 

gtktypebuiltins.h: stamp-gtktypebuiltins.h
	@true
stamp-gtktypebuiltins.h: @REBUILD@ $(gtk_public_h_sources) Makefile
	( cd $(srcdir) && $(GLIB_MKENUMS) \
			--fhead "#ifndef __GTK_TYPE_BUILTINS_H__\n#define __GTK_TYPE_BUILTINS_H__\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n" \
			--fprod "/* enumerations from \"@filename@\" */\n" \
			--vhead "GType @enum_name@_get_type (void) G_GNUC_CONST;\n#define GTK_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n" \
			--ftail "G_END_DECLS\n\n#endif /* __GTK_TYPE_BUILTINS_H__ */" \
		$(gtk_public_h_sources) ) >> xgen-gtbh \
	&& (cmp -s xgen-gtbh gtktypebuiltins.h || cp xgen-gtbh gtktypebuiltins.h ) \
	&& rm -f xgen-gtbh \
	&& echo timestamp > $(@F)
gtktypebuiltins.c: @REBUILD@ $(gtk_public_h_sources) Makefile
	( cd $(srcdir) && $(GLIB_MKENUMS) \
			--fhead "#undef GTK_DISABLE_DEPRECATED\n#define GTK_ENABLE_BROKEN\n#include \"gtk.h\"\n#include \"gtkprivate.h\"\n#include \"gtkalias.h\"\n" \
			--ftail "#define __GTK_TYPE_BUILTINS_C__\n#include \"gtkaliasdef.c\"\n" \
			--fprod "\n/* enumerations from \"@filename@\" */" \
			--vhead "GType\n@enum_name@_get_type (void)\n{\n  static GType etype = 0;\n  if (etype == 0) {\n    static const G@Type@Value values[] = {" \
			--vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
			--vtail "      { 0, NULL, NULL }\n    };\n    etype = g_@type@_register_static (g_intern_static_string (\"@EnumName@\"), values);\n  }\n  return etype;\n}\n" \
		$(gtk_public_h_sources) ) > xgen-gtbc \
	&& cp xgen-gtbc gtktypebuiltins.c  \
	&& rm -f xgen-gtbc

# target platform:
lib_LTLIBRARIES = $(gtktargetlib)

gtkincludedir = $(includedir)/gtk-2.0/gtk
gtkinclude_HEADERS = $(gtk_public_h_sources) $(gtk_semi_private_h_sources) $(gtk_built_public_sources) gtkversion.h

gtkunixprintincludedir = $(includedir)/gtk-unix-print-2.0/gtk
gtkunixprintinclude_HEADERS = $(gtk_unix_print_public_h_sources)

libgtk_x11_2_0_la_SOURCES = $(gtk_c_sources)
libgtk_linux_fb_2_0_la_SOURCES = $(gtk_c_sources)
libgtk_win32_2_0_la_SOURCES = $(gtk_c_sources)
libgtk_quartz_2_0_la_SOURCES = $(gtk_c_sources)
libgtk_directfb_2_0_la_SOURCES = $(gtk_c_sources)

libgtk_x11_2_0_la_LDFLAGS = $(libtool_opts)
libgtk_linux_fb_2_0_la_LDFLAGS = $(libtool_opts)
libgtk_win32_2_0_la_LDFLAGS = $(libtool_opts)
libgtk_quartz_2_0_la_LDFLAGS = $(libtool_opts)
libgtk_directfb_2_0_la_LDFLAGS = $(libtool_opts)

libgtk_x11_2_0_la_LIBADD = $(libadd)
libgtk_linux_fb_2_0_la_LIBADD = $(libadd)
libgtk_win32_2_0_la_LIBADD = $(libadd) -lole32 -lgdi32 -lcomdlg32 -lwinspool -lcomctl32 -luuid
libgtk_win32_2_0_la_DEPENDENCIES = $(gtk_def) $(gtk_win32_res)
libgtk_quartz_2_0_la_LIBADD = $(libadd)
libgtk_directfb_2_0_la_LIBADD = $(libadd)

if USE_WIN32
libgtk_target_ldflags = $(gtk_win32_res_ldflag) $(gtk_win32_symbols)
endif
EXTRA_LTLIBRARIES = libgtk-x11-2.0.la libgtk-linux-fb-2.0.la libgtk-win32-2.0.la libgtk-quartz-2.0.la libgtk-directfb-2.0.la

install-exec-hook: 
if DISABLE_EXPLICIT_DEPS
	$(SHELL) $(top_srcdir)/sanitize-la.sh $(DESTDIR)$(libdir)/$(gtktargetlib)
endif

# Install a RC file for the default GTK+ theme, and key themes
install-data-local: install-ms-lib install-def-file
	$(mkinstalldirs) $(DESTDIR)$(datadir)/themes/Raleigh/gtk-2.0
	$(INSTALL_DATA) $(srcdir)/gtkrc.default $(DESTDIR)$(datadir)/themes/Raleigh/gtk-2.0/gtkrc
	$(mkinstalldirs) $(DESTDIR)$(datadir)/themes/Default/gtk-2.0-key
	$(INSTALL_DATA) $(srcdir)/gtkrc.key.default $(DESTDIR)$(datadir)/themes/Default/gtk-2.0-key/gtkrc
	$(mkinstalldirs) $(DESTDIR)$(datadir)/themes/Emacs/gtk-2.0-key
	$(INSTALL_DATA) $(srcdir)/gtkrc.key.emacs $(DESTDIR)$(datadir)/themes/Emacs/gtk-2.0-key/gtkrc

uninstall-local: uninstall-ms-lib uninstall-def-file
	rm -f $(DESTDIR)$(datadir)/themes/Raleigh/gtk-2.0/gtkrc
	rm -f $(DESTDIR)$(datadir)/themes/Default/gtk-2.0-key/gtkrc
	rm -f $(DESTDIR)$(datadir)/themes/Emacs/gtk-2.0-key/gtkrc

# if srcdir!=builddir, clean out maintainer-clean files from builddir
# this allows dist to pass.
distclean-local:
	if test $(srcdir) != .; then \
	  rm -f $(MAINTAINERCLEANFILES); \
	fi

DEPS = $(gtktargetlib) $(top_builddir)/gdk-pixbuf/libgdk_pixbuf-$(GTK_API_VERSION).la $(top_builddir)/gdk/$(gdktargetlib)

TEST_DEPS = $(DEPS) gtk.immodules

LDADDS =								\
	$(gtktargetlib)							\
	$(top_builddir)/gdk-pixbuf/libgdk_pixbuf-$(GTK_API_VERSION).la	\
	$(top_builddir)/gdk/$(gdktargetlib)

#
# Installed tools
#
bin_PROGRAMS = gtk-query-immodules-2.0 gtk-update-icon-cache

gtk_query_immodules_2_0_DEPENDENCIES = $(DEPS)
gtk_query_immodules_2_0_LDADD = $(LDADDS)

gtk_query_immodules_2_0_SOURCES = queryimmodules.c

gtk_update_icon_cache_LDADD = $(top_builddir)/gdk-pixbuf/libgdk_pixbuf-$(GTK_API_VERSION).la

gtk_update_icon_cache_SOURCES = updateiconcache.c

.PHONY: files test test-debug

files:
	@files=`ls $(DISTFILES) 2> /dev/null `; for p in $$files; do \
	  echo $$p; \
	done

STOCK_ICONS = \
	stock-icons/16/gtk-about.png  			\
	stock-icons/16/gtk-add.png    			\
	stock-icons/16/gtk-cdrom.png  			\
	stock-icons/16/gtk-connect.png        		\
	stock-icons/16/gtk-convert.png        		\
	stock-icons/16/gtk-copy.png   			\
	stock-icons/16/gtk-cut.png    			\
	stock-icons/16/gtk-delete.png 			\
	stock-icons/16/gtk-directory.png      		\
	stock-icons/16/gtk-disconnect.png     		\
	stock-icons/16/gtk-edit.png   			\
	stock-icons/16/gtk-execute.png        		\
	stock-icons/16/gtk-file.png   			\
	stock-icons/16/gtk-find-and-replace.png       	\
	stock-icons/16/gtk-find.png   			\
	stock-icons/16/gtk-floppy.png 			\
	stock-icons/16/gtk-fullscreen.png     		\
	stock-icons/16/gtk-go-back-ltr.png    		\
	stock-icons/16/gtk-go-down.png        		\
	stock-icons/16/gtk-go-forward-ltr.png 		\
	stock-icons/16/gtk-goto-bottom.png    		\
	stock-icons/16/gtk-goto-first-ltr.png 		\
	stock-icons/16/gtk-goto-last-ltr.png  		\
	stock-icons/16/gtk-goto-top.png       		\
	stock-icons/16/gtk-go-up.png  			\
	stock-icons/16/gtk-harddisk.png       		\
	stock-icons/16/gtk-help.png   			\
	stock-icons/16/gtk-home.png   			\
	stock-icons/16/gtk-indent-ltr.png		\
	stock-icons/16/gtk-indent-rtl.png		\
	stock-icons/16/gtk-index.png  			\
	stock-icons/16/gtk-info.png   			\
	stock-icons/16/gtk-jump-to-ltr.png    		\
	stock-icons/16/gtk-jump-to-rtl.png    		\
	stock-icons/16/gtk-justify-center.png 		\
	stock-icons/16/gtk-justify-fill.png   		\
	stock-icons/16/gtk-justify-left.png   		\
	stock-icons/16/gtk-justify-right.png  		\
	stock-icons/16/gtk-leave-fullscreen.png       	\
	stock-icons/16/gtk-media-forward-ltr.png      	\
	stock-icons/16/gtk-media-next-ltr.png 		\
	stock-icons/16/gtk-media-pause.png    		\
	stock-icons/16/gtk-media-play-ltr.png 		\
	stock-icons/16/gtk-media-play-rtl.png 		\
	stock-icons/16/gtk-media-previous-ltr.png     	\
	stock-icons/16/gtk-media-record.png   		\
	stock-icons/16/gtk-media-rewind-ltr.png       	\
	stock-icons/16/gtk-media-stop.png     		\
	stock-icons/16/gtk-missing-image.png  		\
	stock-icons/16/gtk-network.png        		\
	stock-icons/16/gtk-new.png    			\
	stock-icons/16/gtk-open.png   			\
	stock-icons/16/gtk-paste.png  			\
	stock-icons/16/gtk-preferences.png    		\
	stock-icons/16/gtk-print.png  			\
	stock-icons/16/gtk-print-preview.png  		\
	stock-icons/16/gtk-properties.png     		\
	stock-icons/16/gtk-quit.png   			\
	stock-icons/16/gtk-redo-ltr.png       		\
	stock-icons/16/gtk-redo-rtl.png       		\
	stock-icons/16/gtk-refresh.png        		\
	stock-icons/16/gtk-remove.png 			\
	stock-icons/16/gtk-revert-to-saved-ltr.png    	\
	stock-icons/16/gtk-revert-to-saved-rtl.png    	\
	stock-icons/16/gtk-save-as.png        		\
	stock-icons/16/gtk-select-all.png     		\
	stock-icons/16/gtk-select-font.png    		\
	stock-icons/16/gtk-sort-ascending.png	 	\
	stock-icons/16/gtk-sort-descending.png        	\
	stock-icons/16/gtk-spell-check.png    		\
	stock-icons/16/gtk-stop.png   			\
	stock-icons/16/gtk-strikethrough.png  		\
	stock-icons/16/gtk-font.png   			\
	stock-icons/16/gtk-undelete-ltr.png   		\
	stock-icons/16/gtk-undelete-rtl.png   		\
	stock-icons/16/gtk-underline.png      		\
	stock-icons/16/gtk-undo-ltr.png       		\
	stock-icons/16/gtk-undo-rtl.png       		\
	stock-icons/16/gtk-unindent-ltr.png    		\
	stock-icons/16/gtk-unindent-rtl.png    		\
	stock-icons/16/gtk-zoom-100.png       		\
	stock-icons/16/gtk-zoom-fit.png       		\
	stock-icons/16/gtk-zoom-in.png        		\
	stock-icons/16/gtk-zoom-out.png       		\
	stock-icons/16/gtk-italic.png 			\
	stock-icons/16/gtk-bold.png   			\
	stock-icons/20/gtk-apply.png  			\
	stock-icons/20/gtk-cancel.png 			\
	stock-icons/20/gtk-close.png  			\
	stock-icons/20/gtk-no.png     			\
	stock-icons/20/gtk-ok.png     			\
	stock-icons/20/gtk-yes.png    			\
	stock-icons/24/gtk-about.png  			\
	stock-icons/24/gtk-add.png    			\
	stock-icons/24/gtk-bold.png   			\
	stock-icons/24/gtk-cdrom.png  			\
	stock-icons/24/gtk-clear.png  			\
	stock-icons/24/gtk-close.png  			\
	stock-icons/24/gtk-color-picker.png   		\
	stock-icons/24/gtk-connect.png        		\
	stock-icons/24/gtk-convert.png        		\
	stock-icons/24/gtk-copy.png   			\
	stock-icons/24/gtk-cut.png    			\
	stock-icons/24/gtk-directory.png      		\
	stock-icons/24/gtk-disconnect.png     		\
	stock-icons/24/gtk-edit.png   			\
	stock-icons/24/gtk-execute.png        		\
	stock-icons/24/gtk-file.png   			\
	stock-icons/24/gtk-find-and-replace.png       	\
	stock-icons/24/gtk-find.png   			\
	stock-icons/24/gtk-font.png   			\
	stock-icons/24/gtk-fullscreen.png     		\
	stock-icons/24/gtk-go-back-ltr.png    		\
	stock-icons/24/gtk-goto-top.png       		\
	stock-icons/24/gtk-go-down.png        		\
	stock-icons/24/gtk-goto-bottom.png    		\
	stock-icons/24/gtk-goto-first-ltr.png 		\
	stock-icons/24/gtk-goto-last-ltr.png  		\
	stock-icons/24/gtk-go-up.png  			\
	stock-icons/24/gtk-harddisk.png       		\
	stock-icons/24/gtk-help.png   			\
	stock-icons/24/gtk-home.png   			\
	stock-icons/24/gtk-indent-ltr.png 		\
	stock-icons/24/gtk-indent-rtl.png 		\
	stock-icons/24/gtk-index.png  			\
	stock-icons/24/gtk-info.png   			\
	stock-icons/24/gtk-italic.png 			\
	stock-icons/24/gtk-jump-to-ltr.png    		\
	stock-icons/24/gtk-jump-to-rtl.png    		\
	stock-icons/24/gtk-justify-center.png 		\
	stock-icons/24/gtk-justify-fill.png   		\
	stock-icons/24/gtk-justify-left.png   		\
	stock-icons/24/gtk-justify-right.png  		\
	stock-icons/24/gtk-leave-fullscreen.png       	\
	stock-icons/24/gtk-media-forward-ltr.png      	\
	stock-icons/24/gtk-media-next-ltr.png 		\
	stock-icons/24/gtk-media-pause.png    		\
	stock-icons/24/gtk-media-play-ltr.png 		\
	stock-icons/24/gtk-media-play-rtl.png 		\
	stock-icons/24/gtk-media-previous-ltr.png     	\
	stock-icons/24/gtk-media-record.png   		\
	stock-icons/24/gtk-media-rewind-ltr.png       	\
	stock-icons/24/gtk-media-stop.png     		\
	stock-icons/24/gtk-missing-image.png  		\
	stock-icons/24/gtk-network.png        		\
	stock-icons/24/gtk-new.png    			\
	stock-icons/24/gtk-open.png   			\
	stock-icons/24/gtk-orientation-reverse-landscape.png \
	stock-icons/24/gtk-orientation-landscape.png	\
	stock-icons/24/gtk-orientation-reverse-portrait.png	\
	stock-icons/24/gtk-orientation-portrait.png	\
	stock-icons/24/gtk-paste.png  			\
	stock-icons/24/gtk-preferences.png    		\
	stock-icons/24/gtk-print.png  			\
	stock-icons/24/gtk-print-preview.png  		\
	stock-icons/24/gtk-properties.png     		\
	stock-icons/24/gtk-quit.png   			\
	stock-icons/24/gtk-redo-ltr.png       		\
	stock-icons/24/gtk-redo-rtl.png       		\
	stock-icons/24/gtk-refresh.png        		\
	stock-icons/24/gtk-remove.png 			\
	stock-icons/24/gtk-revert-to-saved-ltr.png   	\
	stock-icons/24/gtk-revert-to-saved-rtl.png    	\
	stock-icons/24/gtk-select-font.png    		\
	stock-icons/24/gtk-save-as.png        		\
	stock-icons/24/gtk-floppy.png 			\
	stock-icons/24/gtk-select-all.png     		\
	stock-icons/24/gtk-select-color.png   		\
	stock-icons/24/gtk-sort-ascending.png 		\
	stock-icons/24/gtk-sort-descending.png        	\
	stock-icons/24/gtk-spell-check.png    		\
	stock-icons/24/gtk-stop.png   			\
	stock-icons/24/gtk-strikethrough.png  		\
	stock-icons/24/gtk-delete.png 			\
	stock-icons/24/gtk-undelete-ltr.png   		\
	stock-icons/24/gtk-undelete-rtl.png   		\
	stock-icons/24/gtk-underline.png      		\
	stock-icons/24/gtk-undo-ltr.png       		\
	stock-icons/24/gtk-undo-rtl.png       		\
	stock-icons/24/gtk-unindent-ltr.png    		\
	stock-icons/24/gtk-unindent-rtl.png    		\
	stock-icons/24/gtk-zoom-100.png       		\
	stock-icons/24/gtk-zoom-fit.png       		\
	stock-icons/24/gtk-zoom-in.png        		\
	stock-icons/24/gtk-zoom-out.png       		\
	stock-icons/24/gtk-go-forward-ltr.png 		\
	stock-icons/32/gtk-dnd-multiple.png   		\
	stock-icons/32/gtk-dnd.png    			\
	stock-icons/48/gtk-dialog-authentication.png  	\
	stock-icons/48/gtk-dialog-error.png   		\
	stock-icons/48/gtk-dialog-info.png    		\
	stock-icons/48/gtk-dialog-question.png        	\
	stock-icons/48/gtk-dialog-warning.png

GENERATED_ICONS = \
	stock-icons/16/gtk-go-back-rtl.png    		\
	stock-icons/16/gtk-go-forward-rtl.png 		\
	stock-icons/16/gtk-goto-first-rtl.png 		\
	stock-icons/16/gtk-goto-last-rtl.png  		\
	stock-icons/16/gtk-media-forward-rtl.png      	\
	stock-icons/16/gtk-media-next-rtl.png 		\
	stock-icons/16/gtk-media-previous-rtl.png     	\
	stock-icons/16/gtk-media-rewind-rtl.png       	\
	stock-icons/16/gtk-save.png			\
	stock-icons/24/gtk-go-back-rtl.png    		\
	stock-icons/24/gtk-go-forward-rtl.png 		\
	stock-icons/24/gtk-goto-first-rtl.png 		\
	stock-icons/24/gtk-goto-last-rtl.png  		\
	stock-icons/24/gtk-media-forward-rtl.png      	\
	stock-icons/24/gtk-media-next-rtl.png 		\
	stock-icons/24/gtk-media-previous-rtl.png     	\
	stock-icons/24/gtk-media-rewind-rtl.png       	\
	stock-icons/24/gtk-save.png

# need to copy the icons so that we can replace the generated ones with
# symlinks, even in the readonly srcdir case
stamp-icons: $(STOCK_ICONS)
	if [ ! -d stock-icons ]; then 						\
          cp -R $(srcdir)/stock-icons .; 					\
	  chmod -R u+w stock-icons;						\
        fi &&									\
	for i in 16 24; do							\
	   (cd stock-icons/$$i							\
	   && $(RM) gtk-go-back-rtl.png 					\
	   && $(LN_S) gtk-go-forward-ltr.png gtk-go-back-rtl.png		\
	   && $(RM) gtk-go-forward-rtl.png					\
	   && $(LN_S) gtk-go-back-ltr.png gtk-go-forward-rtl.png		\
	   && $(RM) gtk-goto-first-rtl.png 					\
	   && $(LN_S) gtk-goto-last-ltr.png gtk-goto-first-rtl.png		\
	   && $(RM) gtk-goto-last-rtl.png					\
	   && $(LN_S) gtk-goto-first-ltr.png gtk-goto-last-rtl.png		\
	   && $(RM) gtk-media-forward-rtl.png 					\
	   && $(LN_S) gtk-media-rewind-ltr.png gtk-media-forward-rtl.png	\
	   && $(RM) gtk-media-next-rtl.png					\
	   && $(LN_S) gtk-media-previous-ltr.png gtk-media-next-rtl.png		\
	   && $(RM) gtk-media-previous-rtl.png 					\
	   && $(LN_S) gtk-media-next-ltr.png gtk-media-previous-rtl.png		\
	   && $(RM) gtk-media-rewind-rtl.png					\
	   && $(LN_S) gtk-media-forward-ltr.png gtk-media-rewind-rtl.png	\
	   && $(RM) gtk-save.png						\
	   && $(LN_S) gtk-floppy.png gtk-save.png)				\
	done									\
	&& touch stamp-icons

if CROSS_COMPILING
gtk_update_icon_cache_program = $(GTK_UPDATE_ICON_CACHE)
else
gtk_update_icon_cache_program =							\
	GDK_PIXBUF_MODULE_FILE=$(top_builddir)/gdk-pixbuf/gdk-pixbuf.loaders	\
	   ./gtk-update-icon-cache
endif

gtkbuiltincache.h: @REBUILD@ stamp-icons
	$(MAKE) $(AM_MAKEFLAGS) gtk-update-icon-cache$(EXEEXT)
	$(gtk_update_icon_cache_program) --force --ignore-theme-index		\
	   --source builtin_icons stock-icons > gtkbuiltincache.h.tmp &&        \
	mv gtkbuiltincache.h.tmp gtkbuiltincache.h

EXTRA_DIST +=                   \
	$(STOCK_ICONS)		\
	$(GENERATED_ICONS)	\
	line-arrow.xbm 		\
	line-wrap.xbm 		\
	tree_plus.xbm		\
	tree_minus.xbm          \
	tree_minus.xpm		\
	tree_plus.xpm		\
	gtk.def			\
	gtk-win32.rc		\
	gtk-win32.rc.in		\
	gtkwin32embed.h		\
	gtkwin32embedwidget.h	\
	gtkwin32embedwidget.c	\
	gtkfilesystemwin32.h	\
	gtkfilesystemwin32.c	\
	gtkprint-win32.h	\
	gtkprint-win32.c	\
	gtkrc.default		\
	gtkrc.key.default	\
	gtkrc.key.emacs		\
	makefile.msc		\
	makefile.msc.in		\
	makegtkalias.pl		\
	abicheck.sh		\
	aliasfilescheck.sh	\
	pltcheck.sh

install-data-local:
