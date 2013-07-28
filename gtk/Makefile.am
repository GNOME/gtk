# Makefile.am for gtk+/gtk
include $(top_srcdir)/Makefile.decl
-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = \
	--add-include-path=../gdk
INTROSPECTION_COMPILER_ARGS = \
	--includedir=$(srcdir) \
    --includedir=. \
    --includedir=../gdk

if USE_QUARTZ
GTK_PRINT_PREVIEW_COMMAND="open -a /Applications/Preview.app %f"
else
GTK_PRINT_PREVIEW_COMMAND="evince --unlink-tempfile --preview --print-settings %s %f"
endif

SUBDIRS = theme-bits . tests
DIST_SUBDIRS=theme-bits tests

if HAVE_PAPI_CUPS
GTK_PRINT_BACKENDS=file,papi,cups
else
if HAVE_CUPS
GTK_PRINT_BACKENDS=file,cups
else
if HAVE_PAPI
GTK_PRINT_BACKENDS=file,papi
else
GTK_PRINT_BACKENDS=file,lpr
endif
endif
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
	-I$(top_builddir)				\
	-I$(top_builddir)/gtk				\
	-I$(top_srcdir) -I../gdk			\
	-I$(top_srcdir)/gdk				\
	-DGTK_DISABLE_DEPRECATED			\
	-DGTK_FILE_SYSTEM_ENABLE_UNSUPPORTED		\
	-DGTK_PRINT_BACKEND_ENABLE_UNSUPPORTED		\
	$(GMODULE_CFLAGS)				\
	$(GTK_DEBUG_FLAGS)				\
	$(GTK_DEP_CFLAGS)				\
	$(INCLUDED_IMMODULE_DEFINE)

gtarget=$(gdktarget)

if PLATFORM_WIN32
no_undefined = -no-undefined
endif

if OS_WIN32
gtk_def = gtk.def
gtk_win32_symbols = -export-symbols $(srcdir)/gtk.def

gtk_win32_res = gtk-win32-res.o
gtk_win32_res_ldflag = -Wl,gtk-win32-res.o

gtk-win32-res.o : gtk-win32.rc
	$(WINDRES) gtk-win32.rc $@

install-def-file: gtk.def
	$(INSTALL) $(srcdir)/gtk.def $(DESTDIR)$(libdir)/gtk-win32-2.0.def
uninstall-def-file:
	-rm $(DESTDIR)$(libdir)/gtk-win32-2.0.def
else
install-def-file:
uninstall-def-file:
endif

if MS_LIB_AVAILABLE
noinst_DATA = gtk-win32-$(GTK_API_VERSION).lib

gtk-win32-$(GTK_API_VERSION).lib: libgtk-win32-$(GTK_API_VERSION).la gtk.def
	lib -machine:@LIB_EXE_MACHINE_FLAG@ -name:libgtk-win32-$(GTK_API_VERSION)-@LT_CURRENT_MINUS_AGE@.dll -def:gtk.def -out:$@

install-ms-lib:
	$(INSTALL) gtk-win32-$(GTK_API_VERSION).lib $(DESTDIR)$(libdir)

uninstall-ms-lib:
	-rm $(DESTDIR)$(libdir)/gtk-win32-$(GTK_API_VERSION).lib
else
install-ms-lib:
uninstall-ms-lib:
endif

# This places the generated .def file in srcdir, since it is expected to be there.
# (The one from a tarball is)
gtk.def: gtk.symbols
	(echo -e EXPORTS; $(CPP) -P -DINCLUDE_VARIABLES -DG_OS_WIN32 -DALL_FILES - <$(srcdir)/gtk.symbols | sed -e '/^$$/d' -e 's/^/	/' -e 's/G_GNUC_[^ ]*//g') > $(srcdir)/gtk.def

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

libgtkincludedir = $(includedir)/gtk-2.0/gtk
libadd =								\
	$(top_builddir)/gdk/$(gdktargetlib)				\
	$(GMODULE_LIBS) \
	$(GTK_DEP_LIBS)
deps =

# libtool stuff: set version and export symbols for resolving
# since automake doesn't support conditionalized libsomething_la_LDFLAGS
# we use the general approach here
libtool_opts =							\
  -version-info $(LT_VERSION_INFO)				\
  -export-dynamic $(no_undefined) $(LIBTOOL_EXPORT_OPTIONS)	\
  -rpath $(libdir) $(libgtk_target_ldflags)


included-modules:
if HAVE_INCLUDED_IMMMODULES
	@cd $(top_builddir)/modules/input && $(MAKE) $(AM_MAKEFLAGS) included-modules

libadd += $(INCLUDED_IMMODULE_OBJ)
deps += $(INCLUDED_IMMODULE_OBJ)

$(INCLUDED_IMMODULE_OBJ): included-modules
	@true
endif

.PHONY: included-modules

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
	gtkactivatable.h	\
	gtkadjustment.h		\
	gtkalignment.h		\
	gtkarrow.h		\
	gtkaspectframe.h	\
	gtkassistant.h		\
	gtkbbox.h		\
	gtkbin.h		\
	gtkbindings.h		\
	gtkbox.h		\
	gtkbuilder.h		\
	gtkbuildable.h		\
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
	gtkcellrendererspinner.h\
	gtkcellrenderertext.h	\
	gtkcellrenderertoggle.h	\
	gtkcellview.h		\
	gtkcheckbutton.h	\
	gtkcheckmenuitem.h	\
	gtkclipboard.h		\
	gtkcolorbutton.h	\
	gtkcolorsel.h		\
	gtkcolorseldialog.h	\
	gtkcombobox.h		\
	gtkcomboboxentry.h	\
	gtkcomboboxtext.h	\
	gtkcontainer.h		\
	gtkdebug.h              \
	gtkdialog.h		\
	gtkdnd.h		\
	gtkdrawingarea.h	\
	gtkeditable.h           \
	gtkentry.h		\
	gtkentrybuffer.h	\
	gtkentrycompletion.h	\
	gtkenums.h		\
	gtkeventbox.h		\
	gtkexpander.h		\
	gtkfilechooser.h        \
	gtkfilechooserbutton.h  \
	gtkfilechooserdialog.h  \
	gtkfilechooserwidget.h  \
	gtkfilefilter.h		\
	gtkfixed.h		\
	gtkfontbutton.h		\
	gtkfontsel.h		\
	gtkframe.h		\
	gtkgc.h			\
	gtkhandlebox.h		\
	gtkhbbox.h		\
	gtkhbox.h		\
	gtkhpaned.h		\
	gtkhruler.h		\
	gtkhscale.h		\
	gtkhscrollbar.h		\
	gtkhseparator.h		\
	gtkhsv.h		\
	gtkiconfactory.h	\
	gtkicontheme.h		\
	gtkiconview.h		\
	gtkimage.h		\
	gtkimagemenuitem.h	\
	gtkimcontext.h		\
	gtkimcontextsimple.h	\
	gtkimmodule.h		\
	gtkimmulticontext.h	\
	gtkinfobar.h		\
	gtkinvisible.h		\
	gtkitem.h		\
	gtklabel.h		\
	gtklayout.h             \
	gtklinkbutton.h		\
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
	gtkmountoperation.h     \
	gtknotebook.h		\
	gtkobject.h		\
	gtkoffscreenwindow.h	\
	gtkorientable.h		\
	gtkpagesetup.h		\
	gtkpaned.h		\
	gtkpapersize.h		\
	gtkplug.h		\
	gtkprintcontext.h	\
	gtkprintoperation.h	\
	gtkprintoperationpreview.h	\
	gtkprintsettings.h	\
	gtkprivate.h		\
	gtkprogressbar.h	\
	gtkradioaction.h	\
	gtkradiobutton.h	\
	gtkradiomenuitem.h	\
	gtkradiotoolbutton.h	\
	gtkrange.h		\
	gtkrc.h			\
	gtkrecentaction.h	\
	gtkrecentchooser.h 	\
	gtkrecentchooserdialog.h \
	gtkrecentchoosermenu.h 	\
	gtkrecentchooserwidget.h \
	gtkrecentfilter.h 	\
	gtkrecentmanager.h 	\
	gtkruler.h		\
	gtkscale.h		\
	gtkscalebutton.h	\
	gtkscrollbar.h		\
	gtkscrolledwindow.h	\
	gtkselection.h		\
	gtkseparator.h		\
	gtkseparatormenuitem.h	\
	gtkseparatortoolitem.h	\
	gtkshow.h		\
	gtksettings.h		\
	gtksizegroup.h		\
	gtksocket.h		\
	gtkspinbutton.h		\
	gtkspinner.h		\
	gtkstatusbar.h		\
	gtkstatusicon.h		\
	gtkstock.h		\
	gtkstyle.h		\
	gtktable.h		\
	gtktearoffmenuitem.h    \
	gtktestutils.h		\
	gtktextbuffer.h		\
	gtktextbufferrichtext.h	\
	gtktextchild.h		\
	gtktextdisplay.h	\
	gtktextiter.h		\
	gtktextmark.h		\
	gtktexttag.h		\
	gtktexttagtable.h	\
	gtktextview.h		\
	gtktoggleaction.h	\
	gtktogglebutton.h	\
	gtktoggletoolbutton.h	\
	gtktoolbar.h		\
	gtktoolbutton.h		\
	gtktoolitem.h		\
	gtktoolitemgroup.h	\
	gtktoolpalette.h	\
	gtktoolshell.h		\
	gtktooltip.h		\
	gtktreednd.h		\
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
	gtkvolumebutton.h	\
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
	gtkprintjob.h			\
	gtkunixprint.h
endif

# Installed header files without compatibility guarantees
# that are not included in gtk/gtk.h
gtk_semi_private_h_sources =    \
	gtktextlayout.h

# GTK+ header files that don't get installed
gtk_private_h_sources =		\
	gtkquery.h		\
	gtksearchengine.h	\
	gtksearchenginesimple.h	\
	gtkdndcursors.h		\
	gtkentryprivate.h	\
	gtkbuilderprivate.h 	\
	gtkcustompaperunixdialog.h\
	gtkfilechooserdefault.h	\
	gtkfilechooserembed.h	\
	gtkfilechooserentry.h	\
	gtkfilechooserprivate.h	\
	gtkfilechoosersettings.h \
	gtkfilechooserutils.h	\
	gtkfilesystem.h		\
	gtkfilesystemmodel.h	\
	gtkiconcache.h		\
	gtkimcontextsimpleseqs.h   \
	gtkintl.h		\
	gtkkeyhash.h		\
	gtkmnemonichash.h	\
	gtkmountoperationprivate.h \
	gtkpango.h		\
	gtkpathbar.h		\
	gtkplugprivate.h	\
	gtkprintoperation-private.h\
	gtkprintutils.h		\
	gtkrbtree.h		\
	gtkrecentchooserdefault.h \
	gtkrecentchooserprivate.h \
	gtkrecentchooserutils.h \
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
	gtktoolpaletteprivate.h	\
	gtktreedatalist.h	\
	gtktreeprivate.h	\
	gtkwindow-decorate.h	\
	$(gtk_clipboard_dnd_h_sources)

# GTK+ C sources to build the library from
gtk_base_c_sources =            \
	gtkquery.c		\
	gtksearchengine.c	\
	gtksearchenginesimple.c	\
	fnmatch.c		\
	gtkaboutdialog.c	\
	gtkaccelgroup.c		\
	gtkaccellabel.c		\
	gtkaccelmap.c		\
	gtkaccessible.c		\
	gtkaction.c		\
	gtkactiongroup.c	\
	gtkactivatable.c	\
	gtkadjustment.c		\
	gtkalignment.c		\
	gtkarrow.c		\
	gtkaspectframe.c	\
	gtkassistant.c		\
	gtkbbox.c		\
	gtkbin.c		\
	gtkbindings.c		\
	gtkbox.c		\
	gtkbuildable.c		\
	gtkbuilder.c		\
	gtkbuilderparser.c	\
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
	gtkcellrendererspinner.c\
	gtkcellrenderertext.c	\
	gtkcellrenderertoggle.c	\
	gtkcellview.c		\
	gtkcheckbutton.c	\
	gtkcheckmenuitem.c	\
	gtkcolorbutton.c	\
	gtkcolorsel.c		\
	gtkcolorseldialog.c	\
	gtkcombobox.c		\
	gtkcomboboxentry.c	\
	gtkcomboboxtext.c 	\
	gtkcontainer.c		\
	gtkdialog.c		\
	gtkdrawingarea.c	\
	gtkeditable.c           \
	gtkentry.c		\
	gtkentrybuffer.c	\
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
	gtkfilesystem.c		\
	gtkfilesystemmodel.c	\
	gtkfixed.c		\
	gtkfontbutton.c         \
	gtkfontsel.c            \
	gtkframe.c		\
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
	gtkinfobar.c		\
	gtkinputdialog.c	\
	gtkinvisible.c		\
	gtkitem.c		\
	gtkkeyhash.c		\
	gtklabel.c		\
	gtklayout.c		\
	gtklinkbutton.c		\
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
	gtkmountoperation.c     \
	gtknotebook.c		\
	gtkobject.c		\
	gtkoffscreenwindow.c	\
	gtkorientable.c		\
	gtkpagesetup.c		\
	gtkpaned.c		\
	gtkpango.c		\
	gtkpapersize.c		\
	gtkpathbar.c		\
	gtkplug.c		\
	gtkprintcontext.c	\
	gtkprintoperation.c	\
	gtkprintoperationpreview.c	\
	gtkprintsettings.c	\
	gtkprintutils.c		\
	gtkprogressbar.c	\
	gtkradioaction.c	\
	gtkradiobutton.c	\
	gtkradiomenuitem.c	\
	gtkradiotoolbutton.c	\
	gtkrange.c		\
	gtkrbtree.c 		\
	gtkrc.c			\
	gtkrecentaction.c	\
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
	gtkscalebutton.c	\
	gtkscrollbar.c		\
	gtkscrolledwindow.c	\
	gtkselection.c		\
	gtkseparator.c		\
	gtkseparatormenuitem.c	\
	gtkseparatortoolitem.c	\
	gtksettings.c		\
	gtksizegroup.c		\
	gtkshow.c		\
	gtksocket.c		\
	gtkspinbutton.c		\
	gtkspinner.c		\
	gtkstatusbar.c		\
	gtkstatusicon.c		\
	gtkstock.c		\
	gtkstyle.c		\
	gtktable.c		\
	gtktearoffmenuitem.c    \
	gtktestutils.c		\
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
	gtktoggleaction.c	\
	gtktogglebutton.c	\
	gtktoggletoolbutton.c	\
	gtktoolbar.c		\
	gtktoolbutton.c		\
	gtktoolitem.c		\
	gtktoolitemgroup.c		\
	gtktoolpalette.c	\
	gtktoolshell.c		\
	gtktooltip.c		\
	gtktreedatalist.c	\
	gtktreednd.c		\
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
	gtkvolumebutton.c	\
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

# Broken
gtk_public_h_sources += \
	gtktext.h		\
	gtktree.h		\
	gtktreeitem.h
gtk_base_c_sources += \
	gtktext.c		\
	gtktree.c		\
	gtktreeitem.c

# Deprecated
gtk_public_h_sources += \
	gtkclist.h		\
	gtkcombo.h		\
	gtkctree.h		\
	gtkcurve.h		\
	gtkfilesel.h		\
	gtkgamma.h		\
	gtkinputdialog.h	\
	gtkitemfactory.h	\
	gtklist.h		\
	gtklistitem.h		\
	gtkoldeditable.h	\
	gtkoptionmenu.h		\
	gtkpixmap.h		\
	gtkpreview.h		\
	gtkprogress.h		\
	gtksignal.h		\
	gtktipsquery.h		\
	gtktooltips.h
gtk_base_c_sources += \
	gtkclist.c		\
	gtkcombo.c		\
	gtkctree.c		\
	gtkcurve.c		\
	gtkfilesel.c		\
	gtkgamma.c		\
	gtkitemfactory.c	\
	gtklist.c		\
	gtklistitem.c		\
	gtkoldeditable.c	\
	gtkoptionmenu.c		\
	gtkpixmap.c		\
	gtkpreview.c		\
	gtkprogress.c		\
	gtksignal.c		\
	gtktipsquery.c		\
	gtktooltips.c

gtk_c_sources = $(gtk_base_c_sources)
gtk_all_c_sources = $(gtk_base_c_sources)

gtk_os_unix_c_sources = \
	gtkcustompaperunixdialog.c	\
	gtkpagesetupunixdialog.c	\
	gtkprinter.c			\
	gtkprinteroption.c		\
	gtkprinteroptionset.c		\
	gtkprinteroptionwidget.c	\
	gtkprintjob.c			\
	gtkprintoperation-unix.c	\
	gtkprintunixdialog.c		\
        gtkprintbackend.c		\
	gtksearchenginebeagle.c		\
	gtksearchenginetracker.c
gtk_all_c_sources += $(gtk_os_unix_c_sources)
if OS_UNIX
gtk_private_h_sources += \
	gtkiconcachevalidator.h		\
	gtkprintbackend.h		\
	gtkprinter-private.h		\
	gtkprinteroption.h		\
	gtkprinteroptionset.h		\
	gtkprinteroptionwidget.h	\
	gtksearchenginebeagle.h		\
	gtksearchenginetracker.h
gtk_c_sources += $(gtk_os_unix_c_sources)
endif

gtk_os_win32_c_sources = \
	gtkprint-win32.c		\
	gtkprintoperation-win32.c
gtk_all_c_sources += $(gtk_os_win32_c_sources)
if OS_WIN32
gtk_private_h_sources += gtkprint-win32.h
gtk_c_sources += $(gtk_os_win32_c_sources)
endif

gtk_use_x11_c_sources = \
	gtkplug-x11.c   \
	gtksocket-x11.c \
	gtkxembed.c     \
	gtktrayicon-x11.c \
	gtkmountoperation-x11.c
gtk_use_win32_c_sources = \
	gtkplug-win32.c   \
	gtksocket-win32.c \
	gtkwin32embed.c   \
	gtkwin32embedwidget.c \
	gtkmountoperation-stub.c
gtk_use_quartz_c_sources =	\
	gtksearchenginequartz.c	\
	gtkplug-stub.c		\
	gtksocket-stub.c	\
	gtkmountoperation-stub.c
gtk_use_stub_c_sources = \
	gtkplug-stub.c   \
	gtksocket-stub.c \
	gtkmountoperation-stub.c
gtk_all_c_sources += $(gtk_use_x11_c_sources) $(gtk_use_win32_c_sources) $(gtk_use_stub_c_sources)
if USE_X11
gtk_private_h_sources += gtkxembed.h gtktrayicon.h xembed.h
gtk_c_sources += $(gtk_use_x11_c_sources)
else
if USE_WIN32
gtk_private_h_sources += gtkwin32embed.h gtkwin32embedwidget.h
gtk_c_sources += $(gtk_use_win32_c_sources)
else
if USE_QUARTZ
libgtk_quartz_2_0_la_CFLAGS = "-xobjective-c"
gtk_private_h_sources += gtksearchenginequartz.h
gtk_c_sources += $(gtk_use_quartz_c_sources)
else
gtk_c_sources += $(gtk_use_stub_c_sources)
endif
endif
endif

if USE_QUARTZ
gtk_clipboard_dnd_c_sources = gtkclipboard-quartz.c gtkdnd-quartz.c gtkquartz.c
gtk_clipboard_dnd_h_sources = gtkquartz.h
else
gtk_clipboard_dnd_c_sources = gtkclipboard.c gtkdnd.c
endif
EXTRA_DIST += gtkquartz.h

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
	gtktypefuncs.c				\
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
	gtkstatusicon-quartz.c                  \
	gtk.symbols				\
	gtkversion.h.in				\
	gtkmarshalers.list			\
	gtkmarshal.list

#
# setup GTK+ sources and their dependencies
#
MAINTAINERCLEANFILES = $(gtk_built_sources) $(stamp_files) $(GENERATED_ICONS) stock-icons/icon-theme.cache
EXTRA_HEADERS =
EXTRA_DIST += $(gtk_private_h_sources) $(gtk_extra_sources)
EXTRA_DIST += $(gtk_built_sources)
EXTRA_DIST += $(STOCK_ICONS)

#
# rules to generate built sources
#
# setup autogeneration dependencies
gen_sources = xgen-gdef xgen-gtbh xgen-gtic xgen-gmh xgen-gmc xgen-gmlh xgen-gmlc xgen-gtfsrc.c xgen-gtf
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
stamp-gtktypebuiltins.h: @REBUILD@ $(gtk_public_h_sources) gtktypebuiltins.h.template
	( cd $(srcdir) && $(GLIB_MKENUMS) --template gtktypebuiltins.h.template \
		$(gtk_public_h_sources) ) >> xgen-gtbh \
	&& (cmp -s xgen-gtbh gtktypebuiltins.h || cp xgen-gtbh gtktypebuiltins.h ) \
	&& rm -f xgen-gtbh \
	&& echo timestamp > $(@F)
gtktypebuiltins.c: @REBUILD@ $(gtk_public_h_sources) gtktypebuiltins.c.template
	( cd $(srcdir) && $(GLIB_MKENUMS) --template gtktypebuiltins.c.template \
		$(gtk_public_h_sources) ) > xgen-gtbc \
	&& cp xgen-gtbc gtktypebuiltins.c  \
	&& rm -f xgen-gtbc

gtktypefuncs.c: @REBUILD@ $(top_srcdir)/gtk/*.h $(top_srcdir)/gdk/*.h Makefile
	echo '#include <gtk/gtk.h>' > xgen-gtfsrc.c && \
	  ${CPP} $(DEFS) $(INCLUDES) -DGTK_ENABLE_BROKEN $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) xgen-gtfsrc.c | \
	  grep '^GType\s*g[td]k_[a-zA-Z0-9_]*_get_type\s*(void)' | \
	  grep -o '\bg[td]k_[a-zA-Z0-9_]*_get_type\b' | \
	  sed '{ s/^/*tp++ = /; s/$$/();/; }' | \
	  sort > xgen-gtf \
	&& cp xgen-gtf $@ && rm -f xgen-gtf
gtktestutils.c: gtktypefuncs.c

# target platform:
lib_LTLIBRARIES = $(gtktargetlib)

gtkincludedir = $(includedir)/gtk-2.0/gtk
gtkinclude_HEADERS = $(gtk_public_h_sources) $(gtk_semi_private_h_sources) $(gtk_built_public_sources) gtkversion.h

gtkunixprintincludedir = $(includedir)/gtk-unix-print-2.0/gtk
gtkunixprintinclude_HEADERS = $(gtk_unix_print_public_h_sources)

libgtk_x11_2_0_la_SOURCES = $(gtk_c_sources)
libgtk_win32_2_0_la_SOURCES = $(gtk_c_sources)
libgtk_quartz_2_0_la_SOURCES = $(gtk_c_sources)
libgtk_directfb_2_0_la_SOURCES = $(gtk_c_sources)

libgtk_x11_2_0_la_LDFLAGS = $(libtool_opts)
libgtk_win32_2_0_la_LDFLAGS = $(libtool_opts) -Wl,-luuid
libgtk_quartz_2_0_la_LDFLAGS = $(libtool_opts)
libgtk_directfb_2_0_la_LDFLAGS = $(libtool_opts)

libgtk_x11_2_0_la_LIBADD = $(libadd)
libgtk_win32_2_0_la_LIBADD = $(libadd) -lole32 -lgdi32 -lcomdlg32 -lwinspool -lcomctl32
libgtk_quartz_2_0_la_LIBADD = $(libadd)
libgtk_directfb_2_0_la_LIBADD = $(libadd)

libgtk_x11_2_0_la_DEPENDENCIES = $(deps)
libgtk_win32_2_0_la_DEPENDENCIES = $(gtk_def) $(gtk_win32_res) $(deps)
libgtk_quartz_2_0_la_DEPENDENCIES = $(deps)
libgtk_directfb_2_0_la_DEPENDENCIES = $(deps)

if USE_WIN32
libgtk_target_ldflags = $(gtk_win32_res_ldflag) $(gtk_win32_symbols)
endif
EXTRA_LTLIBRARIES = libgtk-x11-2.0.la libgtk-win32-2.0.la libgtk-quartz-2.0.la libgtk-directfb-2.0.la

install-exec-hook: 
if DISABLE_EXPLICIT_DEPS
	$(SHELL) $(top_srcdir)/sanitize-la.sh $(DESTDIR)$(libdir)/$(gtktargetlib)
endif

if USE_QUARTZ
install-mac-key-theme:
	$(mkinstalldirs) $(DESTDIR)$(datadir)/themes/Mac/gtk-2.0-key
	$(INSTALL_DATA) $(srcdir)/gtkrc.key.mac $(DESTDIR)$(datadir)/themes/Mac/gtk-2.0-key/gtkrc
uninstall-mac-key-theme:
	rm -f $(DESTDIR)$(datadir)/themes/Mac/gtk-2.0-key/gtkrc
else
install-mac-key-theme:
uninstall-mac-key-theme:
endif

dist-hook: ../build/win32/vs9/gtk.vcproj ../build/win32/vs10/gtk.vcxproj ../build/win32/vs10/gtk.vcxproj.filters

../build/win32/vs9/gtk.vcproj: ../build/win32/vs9/gtk.vcprojin
	for F in $(gtk_base_c_sources) $(gtk_os_win32_c_sources) $(gtk_use_win32_c_sources); do \
		case $$F in \
		*.c) echo '   <File RelativePath="..\..\..\gtk\'$$F'" />' \
		     ;; \
		esac; \
	done >libgtk.sourcefiles
	$(CPP) -P - <$(top_srcdir)/build/win32/vs9/gtk.vcprojin >$@
	rm libgtk.sourcefiles

../build/win32/vs10/gtk.vcxproj: ../build/win32/vs10/gtk.vcxprojin
	for F in $(gtk_base_c_sources) $(gtk_os_win32_c_sources) $(gtk_use_win32_c_sources); do \
		case $$F in \
		*.c) echo '   <ClCompile Include="..\..\..\gtk\'$$F'" />' \
		     ;; \
		esac; \
	done >libgtk.vs10.sourcefiles
	$(CPP) -P - <$(top_srcdir)/build/win32/vs10/gtk.vcxprojin >$@
	rm libgtk.vs10.sourcefiles

../build/win32/vs10/gtk.vcxproj.filters: ../build/win32/vs10/gtk.vcxproj.filtersin
	for F in $(gtk_base_c_sources) $(gtk_os_win32_c_sources) $(gtk_use_win32_c_sources); do \
		case $$F in \
		*.c) echo '   <ClCompile Include="..\..\..\gtk\'$$F'"><Filter>Source Files</Filter></ClCompile>' \
		     ;; \
		esac; \
	done >libgtk.vs10.sourcefiles.filters
	$(CPP) -P - <$(top_srcdir)/build/win32/vs10/gtk.vcxproj.filtersin >$@
	rm libgtk.vs10.sourcefiles.filters

# Install a RC file for the default GTK+ theme, and key themes
install-data-local: install-ms-lib install-def-file install-mac-key-theme
	$(mkinstalldirs) $(DESTDIR)$(datadir)/themes/Raleigh/gtk-2.0
	$(INSTALL_DATA) $(srcdir)/gtkrc.default $(DESTDIR)$(datadir)/themes/Raleigh/gtk-2.0/gtkrc
	$(mkinstalldirs) $(DESTDIR)$(datadir)/themes/Default/gtk-2.0-key
	$(INSTALL_DATA) $(srcdir)/gtkrc.key.default $(DESTDIR)$(datadir)/themes/Default/gtk-2.0-key/gtkrc
	$(mkinstalldirs) $(DESTDIR)$(datadir)/themes/Emacs/gtk-2.0-key
	$(INSTALL_DATA) $(srcdir)/gtkrc.key.emacs $(DESTDIR)$(datadir)/themes/Emacs/gtk-2.0-key/gtkrc

uninstall-local: uninstall-ms-lib uninstall-def-file uninstall-mac-key-theme
	rm -f $(DESTDIR)$(datadir)/themes/Raleigh/gtk-2.0/gtkrc
	rm -f $(DESTDIR)$(datadir)/themes/Default/gtk-2.0-key/gtkrc
	rm -f $(DESTDIR)$(datadir)/themes/Emacs/gtk-2.0-key/gtkrc

# if srcdir!=builddir, clean out maintainer-clean files from builddir
# this allows dist to pass.
distclean-local:
	if test $(srcdir) != .; then \
	  rm -f $(MAINTAINERCLEANFILES); \
	fi

DEPS = $(gtktargetlib) $(top_builddir)/gdk/$(gdktargetlib)

TEST_DEPS = $(DEPS) immodules.cache

LDADDS =								\
	$(gtktargetlib)							\
	$(top_builddir)/gdk/$(gdktargetlib)                             \
	$(GTK_DEP_LIBS)

if HAVE_INTROSPECTION
introspection_files = \
    $(gtkinclude_HEADERS) \
    $(gtk_all_c_sources) \
    gtktypebuiltins.h \
    gtktypebuiltins.c

Gtk-2.0.gir: $(INTROSPECTION_SCANNER) $(gtktargetlib) $(top_builddir)/gdk/Gdk-2.0.gir Makefile
Gtk_2_0_gir_SCANNERFLAGS = --warn-all --add-include-path=$(top_builddir)/gdk
if USE_X11
Gtk_2_0_gir_SCANNERFLAGS += --add-include-path=$(top_builddir)/gdk/x11
endif
Gtk_2_0_gir_INCLUDES = Atk-1.0 Gdk-2.0
Gtk_2_0_gir_CFLAGS = \
		$(INCLUDES) \
		-UGDK_DISABLE_DEPRECATED \
		-UGTK_DISABLE_DEPRECATED \
		-DGTK_TEXT_USE_INTERNAL_UNSUPPORTED_API
Gtk_2_0_gir_LIBS = $(gtktargetlib)
Gtk_2_0_gir_FILES = $(introspection_files)
INTROSPECTION_GIRS += Gtk-2.0.gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibsdir = $(libdir)/girepository-1.0
typelibs_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelibs_DATA)
endif

#
# Installed tools
#
bin_PROGRAMS = \
	gtk-query-immodules-2.0 \
	gtk-update-icon-cache

bin_SCRIPTS = gtk-builder-convert

if OS_WIN32

# Workaround for UAC silliness: programs with "update" in their name
# are believed to be installers and require elevated privileges to be
# used... Use a manifest file to tell Windows that
# gtk-update-icon-cache.exe doesn't require any special privileges.

GTK_UPDATE_ICON_CACHE_MANIFEST = gtk-update-icon-cache.exe.manifest

bin_SCRIPTS += \
	$(GTK_UPDATE_ICON_CACHE_MANIFEST)

gtk-update-icon-cache.exe.manifest:
	(echo '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>' ; \
	 echo '<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">' ; \
	 echo '  <assemblyIdentity version="1.0.0.0"' ; \
	 echo '     processorArchitecture="'$(EXE_MANIFEST_ARCHITECTURE)'"' ; \
	 echo '     name="gtk-update-icon-cache.exe"' ; \
	 echo '     type="win32"/>' ; \
	 echo '  <!-- Identify the application security requirements. -->' ; \
	 echo '  <trustInfo xmlns="urn:schemas-microsoft-com:asm.v3">' ; \
	 echo '    <security>' ; \
	 echo '      <requestedPrivileges>' ; \
	 echo '        <requestedExecutionLevel' ; \
	 echo '          level="asInvoker"' ; \
	 echo '          uiAccess="false"/>' ; \
	 echo '        </requestedPrivileges>' ; \
	 echo '       </security>' ; \
	 echo '  </trustInfo>' ; \
	 echo '</assembly>' ) >$@

endif

gtk_query_immodules_2_0_DEPENDENCIES = $(DEPS)
gtk_query_immodules_2_0_LDADD = $(LDADDS) $(GMODULE_LIBS)
gtk_query_immodules_2_0_SOURCES = queryimmodules.c

gtk_update_icon_cache_LDADD = $(GDK_PIXBUF_LIBS)
gtk_update_icon_cache_SOURCES = updateiconcache.c 

.PHONY: files test test-debug

files:
	@files=`ls $(DISTFILES) 2> /dev/null `; for p in $$files; do \
	  echo $$p; \
	done

STOCK_ICONS = \
	stock-icons/16/application-exit.png		\
	stock-icons/16/dialog-information.png		\
	stock-icons/16/document-new.png			\
	stock-icons/16/document-open.png		\
	stock-icons/16/document-open-recent.png		\
	stock-icons/16/document-print.png		\
	stock-icons/16/document-print-preview.png	\
	stock-icons/16/document-properties.png		\
	stock-icons/16/document-revert-ltr.png		\
	stock-icons/16/document-revert-rtl.png		\
	stock-icons/16/document-save-as.png		\
	stock-icons/16/drive-harddisk.png		\
	stock-icons/16/edit-clear.png			\
	stock-icons/16/edit-copy.png			\
	stock-icons/16/edit-cut.png			\
	stock-icons/16/edit-delete.png			\
	stock-icons/16/edit-find.png			\
	stock-icons/16/edit-find-replace.png		\
	stock-icons/16/edit-paste.png			\
	stock-icons/16/edit-redo-ltr.png		\
	stock-icons/16/edit-redo-rtl.png		\
	stock-icons/16/edit-select-all.png		\
	stock-icons/16/edit-undo-ltr.png		\
	stock-icons/16/edit-undo-rtl.png		\
	stock-icons/16/folder.png			\
	stock-icons/16/format-indent-less-ltr.png	\
	stock-icons/16/format-indent-less-rtl.png	\
	stock-icons/16/format-indent-more-ltr.png	\
	stock-icons/16/format-indent-more-rtl.png	\
	stock-icons/16/format-justify-center.png	\
	stock-icons/16/format-justify-fill.png		\
	stock-icons/16/format-justify-left.png		\
	stock-icons/16/format-justify-right.png		\
	stock-icons/16/format-text-bold.png		\
	stock-icons/16/format-text-italic.png		\
	stock-icons/16/format-text-strikethrough.png	\
	stock-icons/16/format-text-underline.png	\
	stock-icons/16/go-previous-ltr.png		\
	stock-icons/16/go-bottom.png			\
	stock-icons/16/go-down.png			\
	stock-icons/16/go-first-ltr.png			\
	stock-icons/16/go-home.png			\
	stock-icons/16/go-jump-ltr.png			\
	stock-icons/16/go-jump-rtl.png			\
	stock-icons/16/go-last-ltr.png			\
	stock-icons/16/go-next-ltr.png			\
	stock-icons/16/go-previous-ltr.png		\
	stock-icons/16/go-top.png			\
	stock-icons/16/go-up.png			\
	stock-icons/16/gtk-caps-lock-warning.png	\
	stock-icons/16/gtk-color-picker.png		\
	stock-icons/16/gtk-connect.png			\
	stock-icons/16/gtk-convert.png			\
	stock-icons/16/gtk-disconnect.png		\
	stock-icons/16/gtk-edit.png			\
	stock-icons/16/gtk-font.png			\
	stock-icons/16/gtk-index.png			\
	stock-icons/16/gtk-orientation-landscape.png	\
	stock-icons/16/gtk-orientation-portrait.png	\
	stock-icons/16/gtk-orientation-reverse-landscape.png \
	stock-icons/16/gtk-orientation-reverse-portrait.png \
	stock-icons/16/gtk-page-setup.png		\
	stock-icons/16/gtk-preferences.png		\
	stock-icons/16/gtk-select-color.png		\
	stock-icons/16/gtk-select-font.png		\
	stock-icons/16/gtk-undelete-ltr.png		\
	stock-icons/16/gtk-undelete-rtl.png		\
	stock-icons/16/help-about.png			\
	stock-icons/16/help-contents.png		\
	stock-icons/16/image-missing.png		\
	stock-icons/16/list-add.png			\
	stock-icons/16/list-remove.png			\
	stock-icons/16/media-floppy.png			\
	stock-icons/16/media-optical.png		\
	stock-icons/16/media-playback-pause.png		\
	stock-icons/16/media-playback-start-ltr.png	\
	stock-icons/16/media-playback-start-rtl.png	\
	stock-icons/16/media-playback-stop.png		\
	stock-icons/16/media-record.png			\
	stock-icons/16/media-seek-backward-ltr.png	\
	stock-icons/16/media-seek-forward-ltr.png	\
	stock-icons/16/media-skip-backward-ltr.png	\
	stock-icons/16/media-skip-forward-ltr.png	\
	stock-icons/16/network-idle.png			\
	stock-icons/16/printer-error.png		\
	stock-icons/16/printer-info.png			\
	stock-icons/16/printer-paused.png		\
	stock-icons/16/printer-warning.png		\
	stock-icons/16/process-stop.png			\
	stock-icons/16/system-run.png			\
	stock-icons/16/text-x-generic.png		\
	stock-icons/16/tools-check-spelling.png		\
	stock-icons/16/view-fullscreen.png		\
	stock-icons/16/view-refresh.png			\
	stock-icons/16/view-restore.png			\
	stock-icons/16/view-sort-ascending.png		\
	stock-icons/16/view-sort-descending.png		\
	stock-icons/16/window-close.png			\
	stock-icons/16/zoom-fit-best.png		\
	stock-icons/16/zoom-in.png			\
	stock-icons/16/zoom-original.png		\
	stock-icons/16/zoom-out.png			\
	\
	stock-icons/20/gtk-apply.png			\
	stock-icons/20/gtk-cancel.png			\
	stock-icons/20/gtk-no.png			\
	stock-icons/20/gtk-ok.png			\
	stock-icons/20/gtk-yes.png			\
	stock-icons/20/window-close.png			\
	\
	stock-icons/24/application-exit.png		\
	stock-icons/24/audio-volume-high.png		\
	stock-icons/24/audio-volume-low.png		\
	stock-icons/24/audio-volume-medium.png		\
	stock-icons/24/audio-volume-muted.png		\
	stock-icons/24/dialog-information.png		\
	stock-icons/24/document-new.png			\
	stock-icons/24/document-open.png		\
	stock-icons/24/document-open-recent.png		\
	stock-icons/24/document-print.png		\
	stock-icons/24/document-print-preview.png	\
	stock-icons/24/document-properties.png		\
	stock-icons/24/document-revert-ltr.png		\
	stock-icons/24/document-revert-rtl.png		\
	stock-icons/24/document-save-as.png		\
	stock-icons/24/drive-harddisk.png		\
	stock-icons/24/edit-clear.png			\
	stock-icons/24/edit-copy.png			\
	stock-icons/24/edit-cut.png			\
	stock-icons/24/edit-delete.png			\
	stock-icons/24/edit-find.png			\
	stock-icons/24/edit-find-replace.png		\
	stock-icons/24/edit-paste.png			\
	stock-icons/24/edit-redo-ltr.png		\
	stock-icons/24/edit-redo-rtl.png		\
	stock-icons/24/edit-select-all.png		\
	stock-icons/24/edit-undo-ltr.png		\
	stock-icons/24/edit-undo-rtl.png		\
	stock-icons/24/folder.png			\
	stock-icons/24/format-indent-less-ltr.png	\
	stock-icons/24/format-indent-less-rtl.png	\
	stock-icons/24/format-indent-more-ltr.png	\
	stock-icons/24/format-indent-more-rtl.png	\
	stock-icons/24/format-justify-center.png	\
	stock-icons/24/format-justify-fill.png		\
	stock-icons/24/format-justify-left.png		\
	stock-icons/24/format-justify-right.png		\
	stock-icons/24/format-text-bold.png		\
	stock-icons/24/format-text-italic.png		\
	stock-icons/24/format-text-strikethrough.png	\
	stock-icons/24/format-text-underline.png	\
	stock-icons/24/go-bottom.png			\
	stock-icons/24/go-down.png			\
	stock-icons/24/go-first-ltr.png			\
	stock-icons/24/go-home.png			\
	stock-icons/24/go-jump-ltr.png			\
	stock-icons/24/go-jump-rtl.png			\
	stock-icons/24/go-last-ltr.png			\
	stock-icons/24/go-next-ltr.png			\
	stock-icons/24/go-previous-ltr.png		\
	stock-icons/24/go-top.png			\
	stock-icons/24/go-up.png			\
	stock-icons/24/gtk-caps-lock-warning.png	\
	stock-icons/24/gtk-color-picker.png		\
	stock-icons/24/gtk-connect.png			\
	stock-icons/24/gtk-convert.png			\
	stock-icons/24/gtk-disconnect.png		\
	stock-icons/24/gtk-edit.png			\
	stock-icons/24/gtk-font.png			\
	stock-icons/24/gtk-index.png			\
	stock-icons/24/gtk-orientation-landscape.png	\
	stock-icons/24/gtk-orientation-portrait.png	\
	stock-icons/24/gtk-orientation-reverse-landscape.png \
	stock-icons/24/gtk-orientation-reverse-portrait.png \
	stock-icons/24/gtk-page-setup.png		\
	stock-icons/24/gtk-preferences.png		\
	stock-icons/24/gtk-select-color.png		\
	stock-icons/24/gtk-select-font.png		\
	stock-icons/24/gtk-undelete-ltr.png		\
	stock-icons/24/gtk-undelete-rtl.png		\
	stock-icons/24/help-about.png			\
	stock-icons/24/help-contents.png		\
	stock-icons/24/image-missing.png		\
	stock-icons/24/list-add.png			\
	stock-icons/24/list-remove.png			\
	stock-icons/24/media-floppy.png			\
	stock-icons/24/media-optical.png		\
	stock-icons/24/media-playback-pause.png		\
	stock-icons/24/media-playback-start-ltr.png	\
	stock-icons/24/media-playback-start-rtl.png	\
	stock-icons/24/media-playback-stop.png		\
	stock-icons/24/media-record.png			\
	stock-icons/24/media-seek-backward-ltr.png	\
	stock-icons/24/media-seek-forward-ltr.png	\
	stock-icons/24/media-skip-backward-ltr.png	\
	stock-icons/24/media-skip-forward-ltr.png	\
	stock-icons/24/network-idle.png			\
	stock-icons/24/printer-error.png		\
	stock-icons/24/printer-info.png			\
	stock-icons/24/printer-paused.png		\
	stock-icons/24/printer-warning.png		\
	stock-icons/24/process-stop.png			\
	stock-icons/24/system-run.png			\
	stock-icons/24/text-x-generic.png		\
	stock-icons/24/tools-check-spelling.png		\
	stock-icons/24/view-fullscreen.png		\
	stock-icons/24/view-refresh.png			\
	stock-icons/24/view-restore.png			\
	stock-icons/24/view-sort-ascending.png		\
	stock-icons/24/view-sort-descending.png		\
	stock-icons/24/window-close.png			\
	stock-icons/24/zoom-fit-best.png		\
	stock-icons/24/zoom-in.png			\
	stock-icons/24/zoom-original.png		\
	stock-icons/24/zoom-out.png			\
	\
	stock-icons/32/gtk-dnd-multiple.png		\
	stock-icons/32/gtk-dnd.png			\
	\
	stock-icons/48/dialog-error.png			\
	stock-icons/48/dialog-information.png		\
	stock-icons/48/dialog-password.png		\
	stock-icons/48/dialog-question.png		\
	stock-icons/48/dialog-warning.png

GENERATED_ICONS = \
	stock-icons/16/go-previous-rtl.png 		\
	stock-icons/16/go-next-rtl.png 			\
	stock-icons/16/go-first-rtl.png 		\
	stock-icons/16/go-last-rtl.png 			\
	stock-icons/16/media-seek-forward-rtl.png 	\
	stock-icons/16/media-skip-forward-rtl.png 	\
	stock-icons/16/media-skip-backward-rtl.png 	\
	stock-icons/16/media-seek-backward-rtl.png 	\
	stock-icons/16/document-save.png 		\
	stock-icons/16/folder-remote.png 		\
	stock-icons/16/user-home.png 			\
	stock-icons/16/user-desktop.png 		\
	stock-icons/24/go-previous-rtl.png 		\
	stock-icons/24/go-next-rtl.png 			\
	stock-icons/24/go-first-rtl.png 		\
	stock-icons/24/go-last-rtl.png 			\
	stock-icons/24/media-seek-forward-rtl.png 	\
	stock-icons/24/media-skip-forward-rtl.png 	\
	stock-icons/24/media-skip-backward-rtl.png 	\
	stock-icons/24/media-seek-backward-rtl.png 	\
	stock-icons/24/document-save.png 		\
	stock-icons/24/folder-remote.png 		\
	stock-icons/24/user-home.png 			\
	stock-icons/24/user-desktop.png

# need to copy the icons so that we can replace the generated ones with
# symlinks, even in the readonly srcdir case
stamp-icons: $(STOCK_ICONS)
	if [ ! -d stock-icons ]; then 					\
	  for d in 16 20 24 32 48; do 					\
	    mkdir -p stock-icons/$$d; 					\
	  done;								\
	  for f in $(STOCK_ICONS); do					\
	    cp $(srcdir)/$$f $$(dirname $$f);				\
	  done;								\
	  chmod -R u+w stock-icons;					\
        fi &&								\
	for i in 16 24; do						\
	   (cd stock-icons/$$i						\
	   && $(RM) document-save.png 					\
	   && $(LN_S) media-floppy.png document-save.png 		\
	   && $(RM) folder-remote.png 					\
	   && $(LN_S) folder.png folder-remote.png 			\
	   && $(RM) go-first-rtl.png 					\
	   && $(LN_S) go-last-ltr.png go-first-rtl.png 			\
	   && $(RM) go-last-rtl.png 					\
	   && $(LN_S) go-first-ltr.png go-last-rtl.png 			\
	   && $(RM) go-next-rtl.png 					\
	   && $(LN_S) go-previous-ltr.png go-next-rtl.png 		\
	   && $(RM) go-previous-rtl.png 				\
	   && $(LN_S) go-next-ltr.png go-previous-rtl.png 		\
	   && $(RM) media-seek-backward-rtl.png 			\
	   && $(LN_S) media-seek-forward-ltr.png media-seek-backward-rtl.png \
	   && $(RM) media-seek-forward-rtl.png 				\
	   && $(LN_S) media-seek-backward-ltr.png media-seek-forward-rtl.png \
	   && $(RM) media-skip-backward-rtl.png 			\
	   && $(LN_S) media-skip-forward-ltr.png media-skip-backward-rtl.png \
	   && $(RM) media-skip-forward-rtl.png 				\
	   && $(LN_S) media-skip-backward-ltr.png media-skip-forward-rtl.png \
	   && $(RM) user-desktop.png 					\
	   && $(LN_S) folder.png user-desktop.png 			\
	   && $(RM) user-home.png 					\
	   && $(LN_S) folder.png user-home.png 				\
	) done 								\
	&& touch stamp-icons

if CROSS_COMPILING
gtk_update_icon_cache_program = $(GTK_UPDATE_ICON_CACHE)
else
gtk_update_icon_cache_program =							\
	   ./gtk-update-icon-cache
endif

gtkbuiltincache.h: @REBUILD@ stamp-icons
	$(MAKE) $(AM_MAKEFLAGS) gtk-update-icon-cache$(EXEEXT) $(GTK_UPDATE_ICON_CACHE_MANIFEST)
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
	gtk-builder-convert	\
	gtk-win32.rc		\
	gtk-win32.rc.in		\
	gtkwin32embed.h		\
	gtkwin32embedwidget.h	\
	gtkwin32embedwidget.c	\
	gtkprint-win32.h	\
	gtkprint-win32.c	\
	gtksearchenginequartz.h \
	gtkrc.default		\
	gtkrc.key.default	\
	gtkrc.key.emacs		\
	gtkrc.key.mac		\
	makefile.msc		\
	makefile.msc.in		\
	makegtkalias.pl		\
	aliasfilescheck.sh	\
	abicheck.sh		\
	pltcheck.sh		\
	gtktypebuiltins.c.template      \
	gtktypebuiltins.h.template


install-data-local:

-include $(top_srcdir)/git.mk
