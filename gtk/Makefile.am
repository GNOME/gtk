# Makefile.am for gtk+/gtk
include $(top_srcdir)/Makefile.decl
-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = \
	--add-include-path=../gdk \
	--warn-all
INTROSPECTION_COMPILER_ARGS = \
	--includedir=$(srcdir) \
    --includedir=. \
    --includedir=../gdk

if USE_QUARTZ
GTK_PRINT_PREVIEW_COMMAND="open -a /Applications/Preview.app %f"
else
GTK_PRINT_PREVIEW_COMMAND="evince --unlink-tempfile --preview --print-settings %s %f"
endif

SUBDIRS = a11y native .

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

AM_CPPFLAGS =						\
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
	-DGTK_FILE_SYSTEM_ENABLE_UNSUPPORTED		\
	-DGTK_PRINT_BACKEND_ENABLE_UNSUPPORTED		\
	$(GMODULE_CFLAGS)				\
	$(GTK_DEBUG_FLAGS)				\
	$(GDK_HIDDEN_VISIBILITY_CFLAGS)			\
	$(GTK_DEP_CFLAGS)				\
	$(INCLUDED_IMMODULE_DEFINE)

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
	$(INSTALL) $(srcdir)/gtk.def $(DESTDIR)$(libdir)/gtk-win32-3.0.def
uninstall-def-file:
	-rm $(DESTDIR)$(libdir)/gtk-win32-3.0.def
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

libgtkincludedir = $(includedir)/gtk-3.0/gtk
libadd = \
	$(top_builddir)/gdk/libgdk-3.la	\
	$(top_builddir)/gtk/a11y/libgtka11y.la \
	$(GMODULE_LIBS) \
	$(GTK_DEP_LIBS)
deps = \
	$(top_builddir)/gdk/libgdk-3.la	\
	$(top_builddir)/gtk/a11y/libgtka11y.la

# libtool stuff: set version and export symbols for resolving
# since automake doesn't support conditionalized libsomething_la_LDFLAGS
# we use the general approach here
libtool_opts =							\
  $(GTK_LINK_FLAGS)						\
  -version-info $(LT_VERSION_INFO)				\
  -export-dynamic $(no_undefined)				\
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

deprecated_h_sources = 			\
	deprecated/gtkcolorsel.h	\
	deprecated/gtkcolorseldialog.h	\
	deprecated/gtkfontsel.h		\
	deprecated/gtkgradient.h	\
	deprecated/gtkhandlebox.h	\
	deprecated/gtkhbbox.h		\
	deprecated/gtkhbox.h		\
	deprecated/gtkhpaned.h		\
	deprecated/gtkhscale.h		\
	deprecated/gtkhscrollbar.h	\
	deprecated/gtkhseparator.h	\
	deprecated/gtkhsv.h		\
	deprecated/gtkrc.h		\
	deprecated/gtkstyle.h		\
	deprecated/gtksymboliccolor.h	\
	deprecated/gtktable.h		\
	deprecated/gtktearoffmenuitem.h	\
	deprecated/gtkvbbox.h		\
	deprecated/gtkvbox.h		\
	deprecated/gtkvscale.h		\
	deprecated/gtkvscrollbar.h	\
	deprecated/gtkvseparator.h	\
	deprecated/gtkvpaned.h

deprecated_private_h_sources =			\
	deprecated/gtkgradientprivate.h		\
	deprecated/gtksymboliccolorprivate.h

gtk_public_h_sources = 		\
	gtk.h			\
	gtkx.h			\
	gtk-a11y.h		\
	gtkaboutdialog.h	\
	gtkaccelgroup.h		\
	gtkaccellabel.h		\
	gtkaccelmap.h		\
	gtkaccessible.h		\
	gtkactionable.h		\
	gtkaction.h		\
	gtkactiongroup.h	\
	gtkactivatable.h	\
	gtkadjustment.h		\
	gtkalignment.h		\
	gtkappchooser.h		\
	gtkappchooserbutton.h	\
	gtkappchooserdialog.h	\
	gtkappchooserwidget.h	\
	gtkapplication.h	\
	gtkapplicationwindow.h	\
	gtkarrow.h		\
	gtkaspectframe.h	\
	gtkassistant.h		\
	gtkbbox.h		\
	gtkbin.h		\
	gtkbindings.h		\
	gtkborder.h		\
	gtkbox.h		\
	gtkbuilder.h		\
	gtkbuildable.h		\
	gtkbutton.h		\
	gtkcalendar.h		\
	gtkcellarea.h		\
	gtkcellareacontext.h	\
	gtkcellareabox.h	\
	gtkcelleditable.h	\
	gtkcelllayout.h		\
	gtkcellrenderer.h	\
	gtkcellrendereraccel.h	\
	gtkcellrenderercombo.h	\
	gtkcellrendererpixbuf.h	\
	gtkcellrendererprogress.h \
	gtkcellrendererspin.h	\
	gtkcellrendererspinner.h\
	gtkcellrenderertext.h	\
	gtkcellrenderertoggle.h	\
	gtkcellview.h		\
	gtkcheckbutton.h	\
	gtkcheckmenuitem.h	\
	gtkclipboard.h		\
	gtkcolorbutton.h	\
	gtkcolorchooser.h	\
	gtkcolorchooserwidget.h	\
	gtkcolorchooserdialog.h	\
	gtkcolorutils.h		\
	gtkcombobox.h		\
	gtkcomboboxtext.h	\
	gtkcontainer.h		\
	gtkcssprovider.h	\
	gtkcsssection.h 	\
	gtkdebug.h		\
	gtkdialog.h		\
	gtkdnd.h		\
	gtkdrawingarea.h	\
	gtkeditable.h		\
	gtkentry.h		\
	gtkentrybuffer.h	\
	gtkentrycompletion.h	\
	gtkenums.h		\
	gtkeventbox.h		\
	gtkexpander.h		\
	gtkfilechooser.h	\
	gtkfilechooserbutton.h	\
	gtkfilechooserdialog.h	\
	gtkfilechooserwidget.h	\
	gtkfilefilter.h		\
	gtkfixed.h		\
	gtkfontbutton.h		\
	gtkfontchooser.h	\
	gtkfontchooserdialog.h  \
	gtkfontchooserwidget.h	\
	gtkframe.h		\
	gtkgrid.h		\
	gtkheaderbar.h		\
	gtkiconfactory.h	\
	gtkicontheme.h		\
	gtkiconview.h		\
	gtkimage.h		\
	gtkimagemenuitem.h	\
	gtkimcontext.h		\
	gtkimcontextinfo.h	\
	gtkimcontextsimple.h	\
	gtkimmodule.h		\
	gtkimmulticontext.h	\
	gtkinfobar.h		\
	gtkinvisible.h		\
	gtklabel.h		\
	gtklayout.h		\
	gtklevelbar.h		\
	gtklinkbutton.h		\
	gtklistbox.h		\
	gtkliststore.h		\
	gtklockbutton.h		\
	gtkmain.h		\
	gtkmenu.h		\
	gtkmenubar.h		\
	gtkmenubutton.h		\
	gtkmenuitem.h		\
	gtkmenushell.h		\
	gtkmenutoolbutton.h	\
	gtkmessagedialog.h	\
	gtkmisc.h		\
	gtkmodules.h		\
	gtkmountoperation.h	\
	gtknotebook.h		\
	gtknumerableicon.h	\
	gtkoffscreenwindow.h	\
	gtkorientable.h		\
	gtkoverlay.h		\
	gtkpagesetup.h		\
	gtkpaned.h		\
	gtkpapersize.h		\
	gtkplacessidebar.h	\
	gtkplug.h		\
	gtkprintcontext.h	\
	gtkprintoperation.h	\
	gtkprintoperationpreview.h \
	gtkprintsettings.h	\
	gtkprogressbar.h	\
	gtkradioaction.h	\
	gtkradiobutton.h	\
	gtkradiomenuitem.h	\
	gtkradiotoolbutton.h	\
	gtkrange.h		\
	gtkrecentaction.h	\
	gtkrecentchooser.h	\
	gtkrecentchooserdialog.h \
	gtkrecentchoosermenu.h	\
	gtkrecentchooserwidget.h \
	gtkrecentfilter.h	\
	gtkrecentmanager.h	\
	gtkrevealer.h		\
	gtkscale.h		\
	gtkscalebutton.h	\
	gtkscrollable.h		\
	gtkscrollbar.h		\
	gtkscrolledwindow.h	\
	gtksearchbar.h		\
	gtksearchentry.h	\
	gtkselection.h		\
	gtkseparator.h		\
	gtkseparatormenuitem.h	\
	gtkseparatortoolitem.h	\
	gtksettings.h		\
	gtkshow.h		\
	gtksizegroup.h		\
	gtksizerequest.h	\
	gtksocket.h		\
	gtkspinbutton.h		\
	gtkspinner.h		\
	gtkstack.h		\
	gtkstackswitcher.h	\
	gtkstatusbar.h		\
	gtkstatusicon.h		\
	gtkstock.h		\
	gtkstylecontext.h	\
	gtkstyleproperties.h	\
	gtkstyleprovider.h	\
	gtkswitch.h		\
	gtktestutils.h		\
	gtktextattributes.h	\
	gtktextbuffer.h		\
	gtktextbufferrichtext.h	\
	gtktextchild.h		\
	gtktextdisplay.h	\
	gtktextiter.h		\
	gtktextmark.h		\
	gtktexttag.h		\
	gtktexttagtable.h	\
	gtktextview.h		\
	gtkthemingengine.h	\
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
	gtktypes.h		\
	gtkuimanager.h		\
	gtkviewport.h		\
	gtkvolumebutton.h	\
	gtkwidget.h		\
	gtkwidgetpath.h		\
	gtkwindow.h

if OS_UNIX
gtk_unix_print_public_h_sources = 	\
	gtkpagesetupunixdialog.h	\
	gtkprintunixdialog.h		\
	gtkprinter.h			\
	gtkprintjob.h			\
	gtkunixprint.h
endif

# Installed header files without compatibility guarantees
# that are not included in gtk/gtk.h
gtk_semi_private_h_sources = \
	gtktextlayout.h

if ENABLE_PACKAGEKIT
gtk_appchooser_impl_h_sources = \
	gtkappchooseronlinepk.h
endif

if ENABLE_PACKAGEKIT
gtk_appchooser_impl_c_sources = \
	gtkappchooseronlinepk.c
endif

gtk_private_type_h_sources =	\
	gtkcsstypesprivate.h	\
	gtktexthandleprivate.h


# GTK+ header files that don't get installed
gtk_private_h_sources =		\
	gtkactionmuxer.h	\
	gtkactionobserver.h	\
	gtkactionobservable.h	\
	gtkapplicationprivate.h	\
	gtkaccelgroupprivate.h	\
	gtkaccelmapprivate.h	\
	gtkactionhelper.h	\
	gtkallocatedbitmaskprivate.h	\
	gtkappchooserprivate.h	\
	gtkappchoosermodule.h	\
	gtkappchooseronline.h	\
	gtkbindingsprivate.h	\
	gtkbitmaskprivate.h	\
	gtkbitmaskprivateimpl.h	\
	gtkbookmarksmanager.h	\
	gtkborderimageprivate.h \
	gtkboxprivate.h         \
	gtkbubblewindowprivate.h	\
	gtkbuilderprivate.h	\
	gtkbuttonprivate.h	\
	gtkcairoblurprivate.h	\
	gtkcellareaboxcontextprivate.h	\
	gtkcolorswatchprivate.h	\
	gtkcoloreditorprivate.h	\
	gtkcolorplaneprivate.h	\
	gtkcolorscaleprivate.h	\
	gtkcolorchooserprivate.h	\
	gtkcontainerprivate.h   \
	gtkcssanimationprivate.h	\
	gtkcssarrayvalueprivate.h	\
	gtkcssbgsizevalueprivate.h	\
	gtkcssbordervalueprivate.h	\
	gtkcsscolorvalueprivate.h	\
	gtkcsscomputedvaluesprivate.h \
	gtkcsscornervalueprivate.h	\
	gtkcsscustompropertyprivate.h \
	gtkcsseasevalueprivate.h	\
	gtkcssenginevalueprivate.h	\
	gtkcssenumvalueprivate.h	\
	gtkcssimagecrossfadeprivate.h	\
	gtkcssimagegradientprivate.h	\
	gtkcssimagelinearprivate.h	\
	gtkcssimageprivate.h	\
	gtkcssimagesurfaceprivate.h	\
	gtkcssimageurlprivate.h	\
	gtkcssimagevalueprivate.h	\
	gtkcssimagewin32private.h	\
	gtkcssinheritvalueprivate.h	\
	gtkcssinitialvalueprivate.h	\
	gtkcsskeyframesprivate.h	\
	gtkcsslookupprivate.h	\
	gtkcssmatcherprivate.h	\
	gtkcssnumbervalueprivate.h	\
	gtkcssparserprivate.h	\
	gtkcsspositionvalueprivate.h	\
	gtkcssproviderprivate.h	\
	gtkcssrepeatvalueprivate.h	\
	gtkcssrgbavalueprivate.h	\
	gtkcsssectionprivate.h 	\
	gtkcssselectorprivate.h	\
	gtkcssshadowsvalueprivate.h	\
	gtkcssshadowvalueprivate.h      \
	gtkcssshorthandpropertyprivate.h \
	gtkcssstringvalueprivate.h	\
	gtkcssstylefuncsprivate.h \
	gtkcssstylepropertyprivate.h \
	gtkcsstransitionprivate.h	\
	gtkcsstypedvalueprivate.h	\
	gtkcssvalueprivate.h	\
	gtkcustompaperunixdialog.h \
	gtkentryprivate.h	\
	gtkfilechooserdefault.h	\
	gtkfilechooserembed.h	\
	gtkfilechooserentry.h	\
	gtkfilechooserprivate.h	\
	gtkfilechooserutils.h	\
	gtkfilesystem.h		\
	gtkfilesystemmodel.h	\
	gtkfontchooserprivate.h	\
	gtkfontchooserutils.h	\
	gtkhslaprivate.h	\
	gtkiconcache.h		\
	gtkiconhelperprivate.h  \
	gtkiconviewprivate.h	\
	gtkimageprivate.h	\
	gtkimmoduleprivate.h	\
	gtkimcontextsimpleseqs.h \
	gtkintl.h		\
	gtkkeyhash.h		\
	gtklockbuttonprivate.h	\
	gtkmenubuttonprivate.h	\
	gtkmenuprivate.h	\
	gtkmenuitemprivate.h	\
	gtkmenushellprivate.h	\
	gtkmenutracker.h	\
	gtkmenutrackeritem.h	\
	gtkmnemonichash.h	\
	gtkmodelmenuitem.h	\
	gtkmodifierstyle.h	\
	gtkmodulesprivate.h	\
	gtkmountoperationprivate.h \
	gtknumerableiconprivate.h \
	gtkorientableprivate.h	\
	gtkpango.h		\
	gtkpathbar.h		\
	gtkpressandholdprivate.h \
	gtkprintoperation-private.h \
	gtkprintutils.h		\
	gtkprivate.h		\
	gtkpixelcacheprivate.h	\
	gtkquery.h		\
	gtkrbtree.h		\
	gtkrecentchooserdefault.h \
	gtkrecentchooserprivate.h \
	gtkrecentchooserutils.h	\
	gtkresources.h		\
	gtkroundedboxprivate.h	\
	gtkscaleprivate.h	\
	gtksearchengine.h	\
	gtksearchenginesimple.h	\
	gtkselectionprivate.h	\
	gtksettingsprivate.h	\
	gtksizegroup-private.h	\
	gtksizerequestcacheprivate.h	\
	gtksocketprivate.h	\
	gtkstyleanimationprivate.h \
	gtkstylecascadeprivate.h \
	gtkstylecontextprivate.h \
	gtkstylepropertiesprivate.h \
	gtkstylepropertyprivate.h \
	gtkstyleproviderprivate.h \
	gtktextbtree.h		\
	gtktextbufferserialize.h \
	gtktextchildprivate.h	\
	gtktexthandleprivate.h	\
	gtktextiterprivate.h	\
	gtktextmarkprivate.h	\
	gtktextsegment.h	\
	gtktexttagprivate.h	\
	gtktexttypes.h		\
	gtktextutil.h		\
	gtkthemingbackgroundprivate.h \
	gtkthemingengineprivate.h \
	gtktrashmonitor.h	\
	gtktoolpaletteprivate.h	\
	gtktreedatalist.h	\
	gtktreeprivate.h	\
	gtkwidgetprivate.h	\
	gtkwin32themeprivate.h	\
	gtkwindowprivate.h	\
	gtktreemenu.h		\
	$(gtk_private_type_h_sources) \
	$(gtk_clipboard_dnd_h_sources) \
	$(gtk_appchooser_impl_h_sources)

# GTK+ C sources to build the library from
deprecated_c_sources = 			\
	deprecated/gtkcolorsel.c	\
	deprecated/gtkcolorseldialog.c	\
	deprecated/gtkfontsel.c		\
	deprecated/gtkgradient.c	\
	deprecated/gtkhandlebox.c	\
	deprecated/gtkhbbox.c		\
	deprecated/gtkhbox.c		\
	deprecated/gtkhpaned.c		\
	deprecated/gtkhscale.c		\
	deprecated/gtkhscrollbar.c	\
	deprecated/gtkhseparator.c	\
	deprecated/gtkhsv.c		\
	deprecated/gtkrc.c		\
	deprecated/gtkstyle.c		\
	deprecated/gtksymboliccolor.c	\
	deprecated/gtktable.c		\
	deprecated/gtktearoffmenuitem.c	\
	deprecated/gtkvbbox.c		\
	deprecated/gtkvbox.c		\
	deprecated/gtkvscale.c		\
	deprecated/gtkvscrollbar.c	\
	deprecated/gtkvseparator.c	\
	deprecated/gtkvpaned.c

gtk_base_c_sources = 		\
	$(deprecated_c_sources)	\
	gtkactionmuxer.c	\
	gtkactionobserver.c	\
	gtkactionobservable.c	\
	gtkactionable.c		\
	gtkquery.c		\
	gtksearchbar.c		\
	gtksearchentry.c	\
	gtksearchengine.c	\
	gtksearchenginesimple.c	\
	fnmatch.c		\
	gtkaboutdialog.c	\
	gtkaccelgroup.c		\
	gtkaccellabel.c		\
	gtkaccelmap.c		\
	gtkaccessible.c		\
	gtkaction.c		\
	gtkactionhelper.c	\
	gtkactiongroup.c	\
	gtkactivatable.c	\
	gtkadjustment.c		\
	gtkalignment.c		\
	gtkallocatedbitmask.c	\
	gtkappchooser.c		\
	gtkappchooserwidget.c	\
	gtkappchooserbutton.c	\
	gtkappchooserdialog.c	\
	gtkappchoosermodule.c	\
	gtkappchooseronline.c	\
	gtkapplication.c	\
	gtkapplicationwindow.c	\
	gtkarrow.c		\
	gtkaspectframe.c	\
	gtkassistant.c		\
	gtkbbox.c		\
	gtkbin.c		\
	gtkbindings.c		\
	gtkbookmarksmanager.c	\
	gtkborder.c		\
	gtkborderimage.c	\
	gtkbox.c		\
	gtkbubblewindow.c	\
	gtkbuildable.c		\
	gtkbuilder.c		\
	gtkbuilderparser.c	\
	gtkbuilder-menus.c	\
	gtkbutton.c		\
	gtkcairoblur.c		\
	gtkcalendar.c		\
	gtkcellarea.c		\
	gtkcellareabox.c	\
	gtkcellareaboxcontext.c	\
	gtkcellareacontext.c	\
	gtkcelleditable.c	\
	gtkcelllayout.c		\
	gtkcellrenderer.c	\
	gtkcellrendereraccel.c	\
	gtkcellrenderercombo.c	\
	gtkcellrendererpixbuf.c	\
	gtkcellrendererprogress.c \
	gtkcellrendererspin.c	\
	gtkcellrendererspinner.c \
	gtkcellrenderertext.c	\
	gtkcellrenderertoggle.c	\
	gtkcellview.c		\
	gtkcheckbutton.c	\
	gtkcheckmenuitem.c	\
	gtkcolorbutton.c	\
	gtkcolorchooser.c	\
	gtkcolorchooserwidget.c	\
	gtkcolorchooserdialog.c	\
	gtkcoloreditor.c	\
	gtkcolorplane.c		\
	gtkcolorscale.c		\
	gtkcolorswatch.c	\
	gtkcolorutils.c		\
	gtkcombobox.c		\
	gtkcomboboxtext.c	\
	gtkcontainer.c		\
	gtkcssanimation.c	\
	gtkcssarrayvalue.c	\
	gtkcssbgsizevalue.c	\
	gtkcssbordervalue.c	\
	gtkcsscolorvalue.c	\
	gtkcsscomputedvalues.c	\
	gtkcsscornervalue.c	\
	gtkcsscustomproperty.c	\
	gtkcsseasevalue.c	\
	gtkcssenumvalue.c	\
	gtkcssenginevalue.c	\
	gtkcssimage.c		\
	gtkcssimagecrossfade.c	\
	gtkcssimagegradient.c	\
	gtkcssimagelinear.c	\
	gtkcssimagesurface.c	\
	gtkcssimageurl.c	\
	gtkcssimagevalue.c	\
	gtkcssimagewin32.c	\
	gtkcssinheritvalue.c	\
	gtkcssinitialvalue.c	\
	gtkcsskeyframes.c	\
	gtkcsslookup.c		\
	gtkcssmatcher.c		\
	gtkcssnumbervalue.c	\
	gtkcssparser.c		\
	gtkcsspositionvalue.c	\
	gtkcssprovider.c	\
	gtkcssrepeatvalue.c	\
	gtkcssrgbavalue.c	\
	gtkcsssection.c 	\
	gtkcssselector.c	\
	gtkcssstringvalue.c	\
	gtkcssshadowsvalue.c	\
	gtkcssshadowvalue.c	\
	gtkcssshorthandproperty.c \
	gtkcssshorthandpropertyimpl.c \
	gtkcssstylefuncs.c	\
	gtkcssstyleproperty.c	\
	gtkcssstylepropertyimpl.c \
	gtkcsstransition.c	\
	gtkcsstypedvalue.c	\
	gtkcssvalue.c		\
	gtkcsstypes.c		\
	gtkdialog.c		\
	gtkdrawingarea.c	\
	gtkeditable.c		\
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
	gtkfilechooserutils.c	\
	gtkfilechooserwidget.c	\
	gtkfilefilter.c		\
	gtkfilesystem.c		\
	gtkfilesystemmodel.c	\
	gtkfixed.c		\
	gtkfontbutton.c		\
	gtkfontchooser.c	\
	gtkfontchooserdialog.c	\
	gtkfontchooserutils.c	\
	gtkfontchooserwidget.c	\
	gtkframe.c		\
	gtkgladecatalog.c	\
	gtkgrid.c		\
	gtkheaderbar.c		\
	gtkhsla.c		\
	gtkiconcache.c		\
	gtkiconcachevalidator.c	\
	gtkiconfactory.c	\
	gtkiconhelper.c		\
	gtkicontheme.c		\
	gtkiconview.c		\
	gtkimage.c		\
	gtkimagemenuitem.c	\
	gtkimcontext.c		\
	gtkimcontextsimple.c	\
	gtkimmodule.c		\
	gtkimmulticontext.c	\
	gtkinfobar.c		\
	gtkinvisible.c		\
	gtkkeyhash.c		\
	gtklabel.c		\
	gtklayout.c		\
	gtklevelbar.c		\
	gtklinkbutton.c		\
	gtklistbox.c		\
	gtkliststore.c		\
	gtklockbutton.c		\
	gtkmain.c		\
	gtkmarshalers.c		\
	gtkmenu.c		\
	gtkmenubar.c		\
	gtkmenubutton.c		\
	gtkmenuitem.c		\
	gtkmenushell.c		\
	gtkmenutracker.c	\
	gtkmenutrackeritem.c	\
	gtkmenutoolbutton.c	\
	gtkmessagedialog.c	\
	gtkmisc.c		\
	gtkmnemonichash.c	\
	gtkmodelmenuitem.c	\
	gtkmodifierstyle.c	\
	gtkmodules.c		\
	gtkmountoperation.c	\
	gtknotebook.c		\
	gtknumerableicon.c	\
	gtkoffscreenwindow.c	\
	gtkorientable.c		\
	gtkoverlay.c		\
	gtkpagesetup.c		\
	gtkpaned.c		\
	gtkpango.c		\
	gtkpapersize.c		\
	gtkpathbar.c		\
	gtkplacessidebar.c	\
	gtkpressandhold.c	\
	gtkprintcontext.c	\
	gtkprintoperation.c	\
	gtkprintoperationpreview.c \
	gtkprintsettings.c	\
	gtkprintutils.c		\
	gtkprivate.c		\
	gtkprivatetypebuiltins.c \
	gtkprogressbar.c	\
	gtkpixelcache.c		\
	gtkradioaction.c	\
	gtkradiobutton.c	\
	gtkradiomenuitem.c	\
	gtkradiotoolbutton.c	\
	gtkrange.c		\
	gtkrbtree.c		\
	gtkrecentaction.c	\
	gtkrecentchooserdefault.c \
	gtkrecentchooserdialog.c \
	gtkrecentchoosermenu.c	\
	gtkrecentchooserwidget.c \
	gtkrecentchooserutils.c	\
	gtkrecentchooser.c	\
	gtkrecentfilter.c	\
	gtkrecentmanager.c	\
	gtkresources.c		\
	gtkrevealer.c		\
	gtkroundedbox.c		\
	gtkscale.c		\
	gtkscalebutton.c	\
	gtkscrollable.c		\
	gtkscrollbar.c		\
	gtkscrolledwindow.c	\
	gtkselection.c		\
	gtkseparator.c		\
	gtkseparatormenuitem.c	\
	gtkseparatortoolitem.c	\
	gtksettings.c		\
	gtksizegroup.c		\
	gtksizerequest.c	\
	gtksizerequestcache.c	\
	gtkshow.c		\
	gtkspinbutton.c		\
	gtkspinner.c		\
	gtkstack.c		\
	gtkstackswitcher.c	\
	gtkstatusbar.c		\
	gtkstatusicon.c		\
	gtkstock.c		\
	gtkstyleanimation.c	\
	gtkstylecascade.c	\
	gtkstylecontext.c	\
	gtkstyleproperties.c	\
	gtkstyleproperty.c	\
	gtkstyleprovider.c	\
	gtkstyleproviderprivate.c	\
	gtkswitch.c		\
	gtktestutils.c		\
	gtktextattributes.c	\
	gtktextbtree.c		\
	gtktextbuffer.c		\
	gtktextbufferrichtext.c	\
	gtktextbufferserialize.c \
	gtktextchild.c		\
	gtktextdisplay.c	\
	gtktexthandle.c		\
	gtktextiter.c		\
	gtktextlayout.c		\
	gtktextmark.c		\
	gtktextsegment.c	\
	gtktexttag.c		\
	gtktexttagtable.c	\
	gtktexttypes.c		\
	gtktextutil.c		\
	gtktextview.c		\
	gtkthemingbackground.c  \
	gtkthemingengine.c	\
	gtktoggleaction.c	\
	gtktogglebutton.c	\
	gtktoggletoolbutton.c	\
	gtktoolbar.c		\
	gtktoolbutton.c		\
	gtktoolitem.c		\
	gtktoolitemgroup.c	\
	gtktoolpalette.c	\
	gtktoolshell.c		\
	gtktooltip.c		\
	gtktrashmonitor.c	\
	gtktreedatalist.c	\
	gtktreednd.c		\
	gtktreemenu.c		\
	gtktreemodel.c		\
	gtktreemodelfilter.c	\
	gtktreemodelsort.c	\
	gtktreeselection.c	\
	gtktreesortable.c	\
	gtktreestore.c		\
	gtktreeview.c		\
	gtktreeviewcolumn.c	\
	gtktypebuiltins.c	\
	gtkuimanager.c		\
	gtkvolumebutton.c	\
	gtkviewport.c		\
	gtkwidget.c		\
	gtkwidgetpath.c		\
	gtkwindow.c		\
	gtkwin32theme.c		\
	$(gtk_clipboard_dnd_c_sources) \
	$(gtk_appchooser_impl_c_sources)

gtk_c_sources = $(gtk_base_c_sources)
gtk_all_c_sources = $(gtk_base_c_sources)

gtk_dbus_built_sources = gtkdbusgenerated.c gtkdbusgenerated.h

$(gtk_dbus_built_sources) : Makefile.am gtkdbusinterfaces.xml
	$(AM_V_GEN) gdbus-codegen \
	--interface-prefix org.Gtk. \
	--c-namespace _Gtk \
	--generate-c-code gtkdbusgenerated \
	$(srcdir)/gtkdbusinterfaces.xml

gtk_os_unix_c_sources = 		\
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
	gtksearchenginetracker.c        \
	$(gtk_dbus_built_sources)
gtk_all_c_sources += $(gtk_os_unix_c_sources)
if OS_UNIX
gtk_private_h_sources += \
	gtkiconcachevalidator.h		\
	gtkprintbackend.h		\
	gtkprinter-private.h		\
	gtkprinteroption.h		\
	gtkprinteroptionset.h		\
	gtkprinteroptionwidget.h	\
	gtksearchenginetracker.h
gtk_c_sources += $(gtk_os_unix_c_sources)
else
gtk_c_sources += $(gtk_dbus_built_sources)
endif

gtk_os_win32_c_sources = \
	gtkprint-win32.c		\
	gtkprintoperation-win32.c	\
	gtkwin32.c
gtk_all_c_sources += $(gtk_os_win32_c_sources)
if OS_WIN32
gtk_private_h_sources += gtkprint-win32.h
gtk_c_sources += $(gtk_os_win32_c_sources)
endif

gtk_use_x11_c_sources = 	\
	gtkplug.c		\
	gtksocket.c		\
	gtkxembed.c		\
	gtktrayicon-x11.c	\
	gtkmountoperation-x11.c
gtk_use_win32_c_sources = 	\
	gtkwin32embed.c		\
	gtkwin32embedwidget.c	\
	gtkmountoperation-stub.c
gtk_use_quartz_c_sources = 	\
	gtksearchenginequartz.c	\
	gtkmountoperation-stub.c \
	gtkmodelmenu-quartz.c \
	gtkquartz.c
gtk_use_stub_c_sources = 	\
	gtkmountoperation-stub.c
gtk_all_c_sources += $(gtk_use_x11_c_sources) $(gtk_use_win32_c_sources) $(gtk_use_quartz_c_sources) $(gtk_use_stub_c_sources)
gtk_use_x11_private_h_sources = 	\
	gtkxembed.h		\
	gtktrayicon.h		\
	xembed.h
if USE_X11
gtk_c_sources += $(gtk_use_x11_c_sources)
gtk_private_h_sources += $(gtk_use_x11_private_h_sources)
endif

gtk_use_win32_private_h_sources = 	\
	gtkwin32embed.h		\
	gtkwin32embedwidget.h
if USE_WIN32
gtk_c_sources += $(gtk_use_win32_c_sources)
gtk_private_h_sources += $(gtk_use_win32_private_h_sources)
endif

gtk_use_quartz_private_h_sources = 	\
	gtksearchenginequartz.h	\
	gtkmodelmenu-quartz.h \
	gtkquartz.h
if USE_QUARTZ
gtk_c_sources += $(gtk_use_quartz_c_sources)
libgtk_3_la_CFLAGS = "-xobjective-c"
gtk_private_h_sources += $(gtk_use_quartz_private_h_sources)
endif

gtk_all_private_h_sources = \
	$(deprecated_private_h_sources) \
	$(gtk_private_h_sources) \
	$(gtk_use_x11_private_h_sources) \
	$(gtk_use_win32_private_h_sources) \
	$(gtk_use_quartz_private_h_sources)

if !USE_X11
if !USE_WIN32
if !USE_QUARTZ
gtk_c_sources += $(gtk_use_stub_c_sources)
endif
endif
endif

if USE_QUARTZ
gtk_clipboard_dnd_c_sources = 	\
	gtkclipboard-quartz.c	\
	gtkdnd-quartz.c
else
if USE_WAYLAND
if !USE_X11
# No wayland gtkdnd-wayland.c yet
gtk_clipboard_dnd_c_sources = 	\
	gtkclipboard-wayland.c \
	gtkdnd.c
else
gtk_clipboard_dnd_c_sources = gtkclipboard.c gtkdnd.c
endif
else
gtk_clipboard_dnd_c_sources = gtkclipboard.c gtkdnd.c
endif
endif

# we use our own built_sources variable rules to avoid automake's
# BUILT_SOURCES oddities
# we generate frequently rebuild files piggyback on a stamp file, so sources
# depending on them only get rebuild when the built source actually changed
# content

# built sources that get installed with the header files
gtk_built_public_sources =			\
	gtktypebuiltins.h

# built headers that don't get installed
gtk_built_private_headers =			\
	gtkdbusgenerated.h			\
	gtkresources.h				\
	gtkmarshalers.h				\
	gtkbuiltincache.h			\
	gtkprivatetypebuiltins.h

gtk_built_sources =				\
	gtkdbusgenerated.c			\
	gtkresources.c				\
	gtktypebuiltins.c			\
	gtktypefuncs.c				\
	gtkmarshalers.c				\
	gtkprivatetypebuiltins.c		\
	${gtk_built_private_headers}		\
	${gtk_built_public_sources}

stamp_files =					\
	stamp-gtkmarshalers.h			\
	stamp-gtktypebuiltins.h			\
	stamp-gtkprivatetypebuiltins.h		\
	stamp-icons

# non-header sources (headers should be specified in the above variables)
# that don't serve as direct make target sources, i.e. they don't have
# their own .lo rules and don't get publically installed
gtk_extra_sources =				\
	paper_names.c				\
	paper_names_offsets.c			\
	gen-paper-names.c			\
	gtkstatusicon-quartz.c			\
	gtk-win32-base.css			\
	gtk-win32.css				\
	gtk-win32-xp.css			\
	gtk-win32-classic.css			\
	gtkversion.h.in				\
	gtkmarshalers.list			\
	fallback-c89.c

#
# setup GTK+ sources and their dependencies
#
MAINTAINERCLEANFILES = \
	$(gtk_built_sources) 			\
	$(stamp_files) 				\
	$(GENERATED_ICONS) 			\
	stock-icons/icon-theme.cache

EXTRA_HEADERS =
EXTRA_DIST += $(gtk_all_private_h_sources) $(gtk_extra_sources)
EXTRA_DIST += $(gtk_built_sources)
EXTRA_DIST += $(STOCK_ICONS)


pkgdatadir=$(datadir)/gtk-$(GTK_API_VERSION)
pkgdata_DATA = gtkbuilder.rng

EXTRA_DIST += gtkbuilder.rnc gtkbuilder.rng

DND_CURSORS = \
	cursor_dnd_ask.png				\
	cursor_dnd_copy.png				\
	cursor_dnd_link.png				\
	cursor_dnd_move.png				\
	cursor_dnd_none.png

COMPOSITE_TEMPLATES =			\
	gtkaboutdialog.ui		\
	gtkappchooserdialog.ui		\
	gtkappchooserwidget.ui		\
	gtkassistant.ui			\
	gtkcolorchooserdialog.ui	\
	gtkcoloreditor.ui		\
	gtkdialog.ui			\
	gtkfilechooserbutton.ui		\
	gtkfilechooserdefault.ui	\
	gtkfilechooserdialog.ui		\
	gtkfontbutton.ui		\
	gtkfontchooserdialog.ui		\
	gtkfontchooserwidget.ui		\
	gtkinfobar.ui			\
	gtklockbutton.ui		\
	gtkmessagedialog.ui		\
	gtkpagesetupunixdialog.ui	\
	gtkpathbar.ui			\
	gtkprintunixdialog.ui		\
	gtkrecentchooserdefault.ui	\
	gtksearchbar.ui			\
	gtkscalebutton.ui		\
	gtkstatusbar.ui			\
	gtkvolumebutton.ui

template_headers = $(COMPOSITE_TEMPLATES:.ui=.ui.h)

MAINTAINERCLEANFILES += $(template_headers)

%.ui.h: %.ui extract-strings$(BUILD_EXEEXT)
	$(AM_V_GEN) ./extract-strings$(BUILD_EXEEXT) $< > $@

#
# rules to generate built sources
#
# setup autogeneration dependencies
gen_sources = xgen-gdef xgen-gtbh xgen-gtic xgen-gmh xgen-gmc xgen-gmlh xgen-gmlc xgen-gtfsrc.c xgen-gtf
CLEANFILES = $(gen_sources)

BUILT_SOURCES = $(gtk_built_sources) stamp-icons

# all autogenerated files need to be generated in the srcdir,
# so old versions get remade and are not confused with newer
# versions in the build dir. thus a development setup requires
# srcdir to be writable, passing --disable-rebuilds to
# ../configure will supress all autogeneration rules.
gtkmarshalers.h: stamp-gtkmarshalers.h
	@true
stamp-gtkmarshalers.h: gtkmarshalers.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --prefix=_gtk_marshal $(srcdir)/gtkmarshalers.list --header --valist-marshallers >> xgen-gmlh \
	&& (cmp -s xgen-gmlh gtkmarshalers.h || cp xgen-gmlh gtkmarshalers.h) \
	&& rm -f xgen-gmlh \
	&& echo timestamp > $(@F)
gtkmarshalers.c: gtkmarshalers.list
	$(AM_V_GEN) (echo "#include \"gtkmarshalers.h\""; \
	 $(GLIB_GENMARSHAL) --prefix=_gtk_marshal $(srcdir)/gtkmarshalers.list --body  --valist-marshallers) >> xgen-gmlc \
	&& cp xgen-gmlc gtkmarshalers.c \
	&& rm -f xgen-gmlc

gtktypebuiltins.h: stamp-gtktypebuiltins.h
	@true
stamp-gtktypebuiltins.h: $(gtk_public_h_sources) $(deprecated_h_sources) gtktypebuiltins.h.template
	$(AM_V_GEN) ( cd $(srcdir) && $(GLIB_MKENUMS) --template gtktypebuiltins.h.template \
		$(gtk_public_h_sources) $(deprecated_h_sources) ) > xgen-gtbh \
	&& (cmp -s xgen-gtbh gtktypebuiltins.h || cp xgen-gtbh gtktypebuiltins.h ) \
	&& rm -f xgen-gtbh \
	&& echo timestamp > $(@F)
gtktypebuiltins.c: $(gtk_public_h_sources) $(deprecated_h_sources) gtktypebuiltins.c.template
	$(AM_V_GEN) ( cd $(srcdir) && $(GLIB_MKENUMS) --template gtktypebuiltins.c.template \
		$(gtk_public_h_sources) $(deprecated_h_sources) ) > xgen-gtbc \
	&& cp xgen-gtbc gtktypebuiltins.c  \
	&& rm -f xgen-gtbc

gtkresources.h: gtk.gresource.xml
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/gtk.gresource.xml \
		--target=$@ --sourcedir=$(srcdir) --c-name _gtk --generate-header --manual-register
gtkresources.c: gtk.gresource.xml gtk-default.css gtk-win32.css gtk-win32-xp.css gtk-win32-base.css gtk-win32-classic.css $(DND_CURSORS) $(COMPOSITE_TEMPLATES) $(template_headers)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $(srcdir)/gtk.gresource.xml \
		--target=$@ --sourcedir=$(srcdir) --c-name _gtk --generate-source --manual-register

gtkprivatetypebuiltins.h: stamp-gtkprivatetypebuiltins.h
	@true
stamp-gtkprivatetypebuiltins.h: $(gtk_private_type_h_sources) gtkprivatetypebuiltins.h.template
	$(AM_V_GEN) ( cd $(srcdir) && $(GLIB_MKENUMS) --template gtkprivatetypebuiltins.h.template \
		$(gtk_private_type_h_sources) ) > xgen-gptbh \
	&& (cmp -s xgen-gptbh gtkprivatetypebuiltins.h || cp xgen-gptbh gtkprivatetypebuiltins.h ) \
	&& rm -f xgen-gptbh \
	&& echo timestamp > $(@F)
gtkprivatetypebuiltins.c: $(gtk_private_type_h_sources)  gtkprivatetypebuiltins.c.template
	$(AM_V_GEN) ( cd $(srcdir) && $(GLIB_MKENUMS) --template gtkprivatetypebuiltins.c.template \
		$(gtk_private_type_h_sources) ) > xgen-gptbc \
	&& cp xgen-gptbc gtkprivatetypebuiltins.c  \
	&& rm -f xgen-gptbc


gtktypefuncs.c: stamp-gtktypebuiltins.h stamp-gtkprivatetypebuiltins.h $(top_srcdir)/gtk/*.h $(top_srcdir)/gtk/deprecated/*.h $(top_srcdir)/gdk/*.h Makefile
	$(AM_V_GEN) echo '#include <gtk/gtkx.h>' > xgen-gtfsrc.c && \
	  echo 'G_GNUC_BEGIN_IGNORE_DEPRECATIONS' > xgen-gtf && \
	  ${CPP} $(DEFS) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) xgen-gtfsrc.c | \
	  $(GREP) -o '\bg[td]k_[a-zA-Z0-9_]*_get_type\b' | \
	  sort | uniq | \
	  $(SED) '{ s/^/*tp++ = /; s/$$/();/; s/^.*\(gdk_x11\|gtk_plug_\|gtk_socket_\).*$$/#ifdef GDK_WINDOWING_X11\n&\n#endif/; }' >> xgen-gtf \
	&& cp xgen-gtf $@ && rm -f xgen-gtf
gtktestutils.c: gtktypefuncs.c

# target platform:
lib_LTLIBRARIES = libgtk-3.la

gtkincludedir = $(includedir)/gtk-3.0/gtk
gtkinclude_HEADERS = $(gtk_public_h_sources) $(gtk_semi_private_h_sources) $(gtk_built_public_sources) gtkversion.h

deprecatedincludedir = $(includedir)/gtk-3.0/gtk/deprecated
deprecatedinclude_HEADERS= $(deprecated_h_sources)

gtkunixprintincludedir = $(includedir)/gtk-3.0/unix-print/gtk
gtkunixprintinclude_HEADERS = $(gtk_unix_print_public_h_sources)

libgtk_3_la_SOURCES = $(gtk_c_sources)
libgtk_3_la_LDFLAGS = $(libtool_opts)
libgtk_3_la_LIBADD = $(libadd)
libgtk_3_la_DEPENDENCIES = $(deps)

if USE_WIN32
libgtk_3_la_LIBADD += -lole32 -lgdi32 -lcomdlg32 -lwinspool -lcomctl32
libgtk_3_la_LDFLAGS += -Wl,-luuid
libgtk_3_la_DEPENDENCIES += $(gtk_def) $(gtk_win32_res) $(deps)
libgtk_target_ldflags = $(gtk_win32_res_ldflag) $(gtk_win32_symbols)
endif

install-exec-hook:
if DISABLE_EXPLICIT_DEPS
	$(SHELL) $(top_srcdir)/sanitize-la.sh $(DESTDIR)$(libdir)/libgtk-3.la
endif

if USE_QUARTZ
install-mac-key-theme:
	$(MKDIR_P) $(DESTDIR)$(datadir)/themes/Mac/gtk-3.0
	$(INSTALL_DATA) $(srcdir)/gtk-keys.css.mac $(DESTDIR)$(datadir)/themes/Mac/gtk-3.0/gtk-keys.css
uninstall-mac-key-theme:
	rm -f $(DESTDIR)$(datadir)/themes/Mac/gtk-3.0/gtk-keys.css
else
install-mac-key-theme:
uninstall-mac-key-theme:
endif

dist-hook: ../build/win32/vs9/gtk.vcproj ../build/win32/vs10/gtk.vcxproj ../build/win32/vs10/gtk.vcxproj.filters

../build/win32/vs9/gtk.vcproj: ../build/win32/vs9/gtk.vcprojin
	for F in `echo $(gtk_base_c_sources) $(gtk_os_win32_c_sources) $(gtk_use_win32_c_sources) $(gtk_dbus_built_sources) | tr '/' '\\'`; do \
		case $$F in \
		*.c) echo '   <File RelativePath="..\..\..\gtk\'$$F'" />' \
		     ;; \
		esac; \
	done >libgtk.sourcefiles
	$(CPP) -P - <$(top_srcdir)/build/win32/vs9/gtk.vcprojin >$@
	rm libgtk.sourcefiles

../build/win32/vs10/gtk.vcxproj: ../build/win32/vs10/gtk.vcxprojin
	for F in `echo $(gtk_base_c_sources) $(gtk_os_win32_c_sources) $(gtk_use_win32_c_sources) $(gtk_dbus_built_sources) | tr '/' '\\'`; do \
		case $$F in \
		*.c) echo '   <ClCompile Include="..\..\..\gtk\'$$F'" />' \
		     ;; \
		esac; \
	done >libgtk.vs10.sourcefiles
	$(CPP) -P - <$(top_srcdir)/build/win32/vs10/gtk.vcxprojin >$@
	rm libgtk.vs10.sourcefiles

../build/win32/vs10/gtk.vcxproj.filters: ../build/win32/vs10/gtk.vcxproj.filtersin
	for F in `echo $(gtk_base_c_sources) $(gtk_os_win32_c_sources) $(gtk_use_win32_c_sources) | tr '/' '\\'`; do \
		case $$F in \
		*.c) echo '   <ClCompile Include="..\..\..\gtk\'$$F'"><Filter>Source Files</Filter></ClCompile>' \
		     ;; \
		esac; \
	done >libgtk.vs10.sourcefiles.filters
	$(CPP) -P - <$(top_srcdir)/build/win32/vs10/gtk.vcxproj.filtersin >$@
	rm libgtk.vs10.sourcefiles.filters

# Install a RC file for the default GTK+ theme, and key themes
install-data-local: install-ms-lib install-def-file install-mac-key-theme
	$(MKDIR_P) $(DESTDIR)$(datadir)/themes/Default/gtk-3.0
	$(INSTALL_DATA) $(srcdir)/gtk-keys.css.default $(DESTDIR)$(datadir)/themes/Default/gtk-3.0/gtk-keys.css
	$(MKDIR_P) $(DESTDIR)$(datadir)/themes/Emacs/gtk-3.0
	$(INSTALL_DATA) $(srcdir)/gtk-keys.css.emacs $(DESTDIR)$(datadir)/themes/Emacs/gtk-3.0/gtk-keys.css

uninstall-local: uninstall-ms-lib uninstall-def-file uninstall-mac-key-theme
	rm -f $(DESTDIR)$(datadir)/themes/Raleigh/gtk-3.0/gtk.css
	rm -f $(DESTDIR)$(datadir)/themes/Default/gtk-3.0/gtk-keys.css
	rm -f $(DESTDIR)$(datadir)/themes/Emacs/gtk-3.0/gtk-keys.css

# if srcdir!=builddir, clean out maintainer-clean files from builddir
# this allows dist to pass.
distclean-local:
	if test $(srcdir) != .; then \
	  rm -f $(MAINTAINERCLEANFILES); \
	  rm -rf stock-icons; \
	  rm -f extract-strings$(EXEEXT); \
	fi

DEPS = libgtk-3.la $(top_builddir)/gdk/libgdk-3.la

TEST_DEPS = $(DEPS) immodules.cache

LDADDS =								\
	libgtk-3.la							\
	$(top_builddir)/gdk/libgdk-3.la				\
	$(GTK_DEP_LIBS)

if HAVE_INTROSPECTION
# The Quartz clipboard and dnd files aren't annotated for
# introspection. Rather than copy the annotations over from the
# regular files, exclude the quartz ones:
introspection_files = \
    $(filter-out %private.h gtktextdisplay.h gtktextlayout.h, $(gtkinclude_HEADERS) $(deprecatedinclude_HEADERS)) \
    $(filter-out  gtkclipboard-quartz.c gtkdnd-quartz.c, \
    $(gtk_base_c_sources)) \
    gtkprintoperation-unix.c \
    gtktypebuiltins.h \
    gtktypebuiltins.c

# And include the regular ones:
if USE_QUARTZ
introspection_files += \
    gtkclipboard.c \
    gtkdnd.c
endif

if USE_X11
introspection_files += \
    gtksocket.c \
    gtkplug.c
endif

Gtk-3.0.gir: $(INTROSPECTION_SCANNER) libgtk-3.la $(top_builddir)/gdk/Gdk-3.0.gir Makefile
Gtk_3_0_gir_SCANNERFLAGS = 			\
	--add-include-path=$(top_builddir)/gdk	\
	--include-uninstalled=$(top_builddir)/gdk/Gdk-3.0.gir
Gtk_3_0_gir_INCLUDES = Atk-1.0
if USE_X11
Gtk_3_0_gir_SCANNERFLAGS += --add-include-path=$(top_builddir)/gdk/x11
Gtk_3_0_gir_INCLUDES += xlib-2.0
endif
Gtk_3_0_gir_SCANNERFLAGS += --c-include="gtk/gtkx.h"
Gtk_3_0_gir_CFLAGS = \
		$(AM_CPPFLAGS) \
		-DGTK_TEXT_USE_INTERNAL_UNSUPPORTED_API
Gtk_3_0_gir_LIBS = libgtk-3.la $(top_builddir)/gdk/libgdk-3.la
Gtk_3_0_gir_FILES = $(introspection_files)
Gtk_3_0_gir_EXPORT_PACKAGES = gtk+-3.0
INTROSPECTION_GIRS += Gtk-3.0.gir

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
	gtk-query-immodules-3.0	\
	gtk-launch

if BUILD_ICON_CACHE
bin_PROGRAMS += gtk-update-icon-cache

if OS_WIN32

# Workaround for UAC silliness: programs with "update" in their name
# are believed to be installers and require elevated privileges to be
# used... Use a manifest file to tell Windows that
# gtk-update-icon-cache.exe doesn't require any special privileges.

GTK_UPDATE_ICON_CACHE_MANIFEST = gtk-update-icon-cache.exe.manifest

bin_SCRIPTS = $(GTK_UPDATE_ICON_CACHE_MANIFEST)

$(GTK_UPDATE_ICON_CACHE_MANIFEST):
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
endif

gtk_query_immodules_3_0_DEPENDENCIES = $(DEPS)
gtk_query_immodules_3_0_LDADD = $(LDADDS) $(GMODULE_LIBS)
gtk_query_immodules_3_0_SOURCES = queryimmodules.c

if BUILD_ICON_CACHE
gtk_update_icon_cache_LDADD = $(GDK_PIXBUF_LIBS)
gtk_update_icon_cache_SOURCES = updateiconcache.c
endif

gtk_launch_LDADD = $(LDADDS)
gtk_launch_SOURCES = gtk-launch.c

# The extract_strings tool is a build utility that runs on the build system.
extract_strings_sources = extract-strings.c
extract_strings_cppflags =
extract_strings_cflags = $(GLIB_CFLAGS_FOR_BUILD)
extract_strings_ldadd = $(GLIB_LIBS_FOR_BUILD)
extract-strings$(BUILD_EXEEXT): $(extract_strings_sources)
	@rm -f extract-strings$(BUILD_EXEEXT)
	$(AM_V_CCLD)$(CC_FOR_BUILD) $(extract_strings_cppflags) $(CPPFLAGS_FOR_BUILD) $(extract_strings_cflags) $(CFLAGS_FOR_BUILD) $^ $(LDFLAGS_FOR_BUILD) $(extract_strings_ldadd) $(LIBS_FOR_BUILD) -o $@
EXTRA_DIST += $(extract_strings_sources)

.PHONY: files test test-debug

files:
	@files=`ls $(DISTFILES) 2> /dev/null `; for p in $$files; do \
	  echo $$p; \
	done

STOCK_ICONS = \
	stock-icons/16/document-open-recent.png		\
	stock-icons/16/help-about.png 			\
	stock-icons/16/list-add.png 			\
	stock-icons/16/media-optical.png 		\
	stock-icons/16/window-close.png 		\
	stock-icons/16/window-minimize.png 		\
	stock-icons/16/window-maximize.png 		\
	stock-icons/16/window-restore.png 		\
	stock-icons/16/window-delete.png 		\
	stock-icons/16/gtk-connect.png 			\
	stock-icons/16/gtk-convert.png 			\
	stock-icons/16/edit-clear.png 			\
	stock-icons/16/edit-copy.png 			\
	stock-icons/16/edit-cut.png 			\
	stock-icons/16/edit-delete.png 			\
	stock-icons/16/folder.png 			\
	stock-icons/16/gtk-disconnect.png 		\
	stock-icons/16/gtk-edit.png 			\
	stock-icons/16/system-run.png 			\
	stock-icons/16/text-x-generic.png 		\
	stock-icons/16/edit-find-replace.png 		\
	stock-icons/16/edit-find.png 			\
	stock-icons/16/media-floppy.png 		\
	stock-icons/16/view-fullscreen.png 		\
	stock-icons/16/go-previous-ltr.png 		\
	stock-icons/16/go-down.png 			\
	stock-icons/16/go-next-ltr.png 			\
	stock-icons/16/go-bottom.png 			\
	stock-icons/16/go-first-ltr.png 		\
	stock-icons/16/go-last-ltr.png 			\
	stock-icons/16/go-top.png 			\
	stock-icons/16/go-up.png 			\
	stock-icons/16/gtk-caps-lock-warning.png	\
	stock-icons/16/gtk-color-picker.png		\
	stock-icons/16/drive-harddisk.png 		\
	stock-icons/16/help-contents.png 		\
	stock-icons/16/go-home.png 			\
	stock-icons/16/format-indent-more-ltr.png 	\
	stock-icons/16/format-indent-more-rtl.png 	\
	stock-icons/16/gtk-index.png 			\
	stock-icons/16/dialog-information.png		\
	stock-icons/16/go-jump-ltr.png 			\
	stock-icons/16/go-jump-rtl.png 			\
	stock-icons/16/format-justify-center.png 	\
	stock-icons/16/format-justify-fill.png 		\
	stock-icons/16/format-justify-left.png 		\
	stock-icons/16/format-justify-right.png 	\
	stock-icons/16/view-restore.png 		\
	stock-icons/16/media-seek-forward-ltr.png 	\
	stock-icons/16/media-skip-forward-ltr.png 	\
	stock-icons/16/media-playback-pause.png 	\
	stock-icons/16/media-playback-start-ltr.png 	\
	stock-icons/16/media-playback-start-rtl.png 	\
	stock-icons/16/media-skip-backward-ltr.png 	\
	stock-icons/16/media-record.png 		\
	stock-icons/16/media-seek-backward-ltr.png 	\
	stock-icons/16/media-playback-stop.png 		\
	stock-icons/16/image-missing.png 		\
	stock-icons/16/network-idle.png 		\
	stock-icons/16/document-new.png 		\
	stock-icons/16/document-open.png 		\
	stock-icons/16/gtk-orientation-landscape.png	\
	stock-icons/16/gtk-orientation-portrait.png	\
	stock-icons/16/gtk-orientation-reverse-landscape.png \
	stock-icons/16/gtk-orientation-reverse-portrait.png \
	stock-icons/16/gtk-page-setup.png 		\
	stock-icons/16/edit-paste.png 			\
	stock-icons/16/gtk-preferences.png 		\
	stock-icons/16/document-print.png 		\
	stock-icons/16/printer-error.png 		\
	stock-icons/16/printer-paused.png 		\
	stock-icons/16/document-print-preview.png 	\
	stock-icons/16/printer-info.png 		\
	stock-icons/16/printer-warning.png 		\
	stock-icons/16/document-properties.png 		\
	stock-icons/16/application-exit.png 		\
	stock-icons/16/edit-redo-ltr.png 		\
	stock-icons/16/edit-redo-rtl.png 		\
	stock-icons/16/view-refresh.png 		\
	stock-icons/16/list-remove.png 			\
	stock-icons/16/document-revert-ltr.png 		\
	stock-icons/16/document-revert-rtl.png 		\
	stock-icons/16/document-save-as.png 		\
	stock-icons/16/edit-select-all.png 		\
	stock-icons/16/gtk-select-color.png 		\
	stock-icons/16/gtk-select-font.png 		\
	stock-icons/16/view-sort-ascending.png 		\
	stock-icons/16/view-sort-descending.png 	\
	stock-icons/16/tools-check-spelling.png 	\
	stock-icons/16/process-stop.png 		\
	stock-icons/16/format-text-strikethrough.png 	\
	stock-icons/16/gtk-font.png 			\
	stock-icons/16/gtk-undelete-ltr.png 		\
	stock-icons/16/gtk-undelete-rtl.png 		\
	stock-icons/16/format-text-underline.png 	\
	stock-icons/16/edit-undo-ltr.png 		\
	stock-icons/16/edit-undo-rtl.png 		\
	stock-icons/16/format-indent-less-ltr.png 	\
	stock-icons/16/format-indent-less-rtl.png 	\
	stock-icons/16/zoom-original.png 		\
	stock-icons/16/zoom-fit-best.png 		\
	stock-icons/16/zoom-in.png 			\
	stock-icons/16/zoom-out.png 			\
	stock-icons/16/format-text-italic.png 		\
	stock-icons/16/format-text-bold.png 		\
	stock-icons/20/gtk-apply.png  			\
	stock-icons/20/gtk-cancel.png 			\
	stock-icons/20/window-close.png 		\
	stock-icons/20/gtk-no.png 			\
	stock-icons/20/gtk-ok.png 			\
	stock-icons/20/gtk-yes.png 			\
	stock-icons/24/audio-volume-high.png		\
	stock-icons/24/audio-volume-low.png		\
	stock-icons/24/audio-volume-medium.png		\
	stock-icons/24/audio-volume-muted.png		\
	stock-icons/24/document-open-recent.png		\
	stock-icons/24/help-about.png 			\
	stock-icons/24/list-add.png 			\
	stock-icons/24/format-text-bold.png 		\
	stock-icons/24/media-optical.png 		\
	stock-icons/24/edit-clear.png 			\
	stock-icons/24/window-close.png 		\
	stock-icons/24/gtk-caps-lock-warning.png	\
	stock-icons/24/gtk-color-picker.png 		\
	stock-icons/24/gtk-connect.png 			\
	stock-icons/24/gtk-convert.png 			\
	stock-icons/24/edit-copy.png 			\
	stock-icons/24/edit-cut.png 			\
	stock-icons/24/folder.png 			\
	stock-icons/24/gtk-disconnect.png 		\
	stock-icons/24/gtk-edit.png 			\
	stock-icons/24/system-run.png 			\
	stock-icons/24/text-x-generic.png 		\
	stock-icons/24/edit-find-replace.png 		\
	stock-icons/24/edit-find.png 			\
	stock-icons/24/gtk-font.png 			\
	stock-icons/24/view-fullscreen.png 		\
	stock-icons/24/go-previous-ltr.png 		\
	stock-icons/24/go-top.png 			\
	stock-icons/24/go-down.png 			\
	stock-icons/24/go-bottom.png 			\
	stock-icons/24/go-first-ltr.png 		\
	stock-icons/24/go-last-ltr.png 			\
	stock-icons/24/go-up.png 			\
	stock-icons/24/drive-harddisk.png 		\
	stock-icons/24/help-contents.png 		\
	stock-icons/24/go-home.png 			\
	stock-icons/24/format-indent-more-ltr.png 	\
	stock-icons/24/format-indent-more-rtl.png 	\
	stock-icons/24/gtk-index.png  			\
	stock-icons/24/dialog-information.png		\
	stock-icons/24/format-text-italic.png 		\
	stock-icons/24/go-jump-ltr.png 			\
	stock-icons/24/go-jump-rtl.png 			\
	stock-icons/24/format-justify-center.png 	\
	stock-icons/24/format-justify-fill.png 		\
	stock-icons/24/format-justify-left.png 		\
	stock-icons/24/format-justify-right.png		\
	stock-icons/24/view-restore.png 		\
	stock-icons/24/media-seek-forward-ltr.png 	\
	stock-icons/24/media-skip-forward-ltr.png 	\
	stock-icons/24/media-playback-pause.png 	\
	stock-icons/24/media-playback-start-ltr.png 	\
	stock-icons/24/media-playback-start-rtl.png 	\
	stock-icons/24/media-skip-backward-ltr.png 	\
	stock-icons/24/media-record.png 		\
	stock-icons/24/media-seek-backward-ltr.png 	\
	stock-icons/24/media-playback-stop.png 		\
	stock-icons/24/image-missing.png 		\
	stock-icons/24/network-idle.png 		\
	stock-icons/24/document-new.png 		\
	stock-icons/24/document-open.png 		\
	stock-icons/24/gtk-orientation-reverse-landscape.png \
	stock-icons/24/gtk-orientation-landscape.png	\
	stock-icons/24/gtk-orientation-reverse-portrait.png \
	stock-icons/24/gtk-orientation-portrait.png	\
	stock-icons/24/gtk-page-setup.png 		\
	stock-icons/24/edit-paste.png 			\
	stock-icons/24/gtk-preferences.png 		\
	stock-icons/24/document-print.png 		\
	stock-icons/24/printer-error.png 		\
	stock-icons/24/printer-paused.png 		\
	stock-icons/24/document-print-preview.png 	\
	stock-icons/24/printer-info.png 		\
	stock-icons/24/printer-warning.png 		\
	stock-icons/24/document-properties.png 		\
	stock-icons/24/application-exit.png 		\
	stock-icons/24/edit-redo-ltr.png 		\
	stock-icons/24/edit-redo-rtl.png 		\
	stock-icons/24/view-refresh.png 		\
	stock-icons/24/list-remove.png 			\
	stock-icons/24/document-revert-ltr.png 		\
	stock-icons/24/document-revert-rtl.png 		\
	stock-icons/24/gtk-select-font.png 		\
	stock-icons/24/document-save-as.png 		\
	stock-icons/24/media-floppy.png 		\
	stock-icons/24/edit-select-all.png 		\
	stock-icons/24/gtk-select-color.png   		\
	stock-icons/24/view-sort-ascending.png 		\
	stock-icons/24/view-sort-descending.png 	\
	stock-icons/24/tools-check-spelling.png 	\
	stock-icons/24/process-stop.png 		\
	stock-icons/24/format-text-strikethrough.png 	\
	stock-icons/24/edit-delete.png 			\
	stock-icons/24/gtk-undelete-ltr.png 		\
	stock-icons/24/gtk-undelete-rtl.png 		\
	stock-icons/24/format-text-underline.png 	\
	stock-icons/24/edit-undo-ltr.png 		\
	stock-icons/24/edit-undo-rtl.png 		\
	stock-icons/24/format-indent-less-ltr.png 	\
	stock-icons/24/format-indent-less-rtl.png 	\
	stock-icons/24/zoom-original.png 		\
	stock-icons/24/zoom-fit-best.png 		\
	stock-icons/24/zoom-in.png 			\
	stock-icons/24/zoom-out.png 			\
	stock-icons/24/go-next-ltr.png 			\
	stock-icons/32/gtk-dnd-multiple.png 		\
	stock-icons/32/gtk-dnd.png 			\
	stock-icons/48/dialog-password.png 		\
	stock-icons/48/dialog-error.png 		\
	stock-icons/48/dialog-information.png		\
	stock-icons/48/dialog-question.png 		\
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
	$(AM_V_GEN) if [ ! -d stock-icons ]; then 			\
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
	   && $(RM) go-previous-rtl.png 				\
	   && $(LN_S) go-next-ltr.png go-previous-rtl.png 		\
	   && $(RM) go-next-rtl.png 					\
	   && $(LN_S) go-previous-ltr.png go-next-rtl.png 		\
	   && $(RM) go-first-rtl.png 					\
	   && $(LN_S) go-last-ltr.png go-first-rtl.png 			\
	   && $(RM) go-last-rtl.png 					\
	   && $(LN_S) go-first-ltr.png go-last-rtl.png 			\
	   && $(RM) media-seek-forward-rtl.png 				\
	   && $(LN_S) media-seek-backward-ltr.png media-seek-forward-rtl.png \
	   && $(RM) media-skip-forward-rtl.png 				\
	   && $(LN_S) media-skip-backward-ltr.png media-skip-forward-rtl.png \
	   && $(RM) media-skip-backward-rtl.png 			\
	   && $(LN_S) media-skip-forward-ltr.png media-skip-backward-rtl.png \
	   && $(RM) media-seek-backward-rtl.png 			\
	   && $(LN_S) media-seek-forward-ltr.png media-seek-backward-rtl.png \
	   && $(RM) document-save.png 					\
	   && $(LN_S) media-floppy.png document-save.png 		\
	   && $(RM) folder-remote.png 					\
	   && $(LN_S) folder.png folder-remote.png 			\
	   && $(RM) user-home.png 					\
	   && $(LN_S) folder.png user-home.png 				\
	   && $(RM) user-desktop.png 					\
	   && $(LN_S) folder.png user-desktop.png 			\
	) done 								\
	&& touch stamp-icons

if USE_EXTERNAL_ICON_CACHE
gtk_update_icon_cache_program = $(GTK_UPDATE_ICON_CACHE)
else
if CROSS_COMPILING
gtk_update_icon_cache_program = ./native/native-update-icon-cache
else
gtk_update_icon_cache_program = ./gtk-update-icon-cache$(EXEEXT)
endif
endif

gtkbuiltincache.h: stamp-icons
if !USE_EXTERNAL_ICON_CACHE
if CROSS_COMPILING
	$(AM_V_at) $(MAKE) $(AM_MAKEFLAGS) -C native/
else
	$(AM_V_at) $(MAKE) $(AM_MAKEFLAGS) gtk-update-icon-cache$(EXEEXT)
endif
endif
	$(AM_V_GEN) $(gtk_update_icon_cache_program) --quiet --force --ignore-theme-index		\
	   --source builtin_icons stock-icons > gtkbuiltincache.h.tmp &&        \
	mv gtkbuiltincache.h.tmp gtkbuiltincache.h

EXTRA_DIST +=                   \
	$(STOCK_ICONS)		\
	$(COMPOSITE_TEMPLATES)	\
	$(DND_CURSORS)		\
	$(GENERATED_ICONS)	\
	gtk-win32.rc		\
	gtk-win32.rc.in		\
	gtkwin32embed.h		\
	gtkwin32embedwidget.h	\
	gtkwin32embedwidget.c	\
	gtkprint-win32.h	\
	gtkprint-win32.c	\
	gtksearchenginequartz.h \
	gtk.gresource.xml	\
	gtkdbusinterfaces.xml	\
	gtk-default.css		\
	gtk-keys.css.default	\
	gtk-keys.css.emacs	\
	gtk-keys.css.mac	\
	makefile.msc		\
	makefile.msc.in		\
	gtktypebuiltins.c.template \
	gtktypebuiltins.h.template \
	gtkprivatetypebuiltins.c.template \
	gtkprivatetypebuiltins.h.template \
	org.gtk.Settings.FileChooser.gschema.xml \
	org.gtk.Settings.ColorChooser.gschema.xml

gsettings_SCHEMAS = \
	org.gtk.Settings.FileChooser.gschema.xml \
	org.gtk.Settings.ColorChooser.gschema.xml

@GSETTINGS_RULES@

install-data-local:

-include $(top_srcdir)/git.mk
