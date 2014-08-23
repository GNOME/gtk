# Makefile.am for gtk+/gtk
include $(top_srcdir)/Makefile.decl
-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ENV = \
	CC="$(CC)"
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

SUBDIRS = inspector native .
if OS_DARWIN
  ECHO="echo"
else
  ECHO=echo -e
endif

if HAVE_CLOUDPRINT
if HAVE_PAPI_CUPS
GTK_PRINT_BACKENDS=file,papi,cups,cloudprint
else
if HAVE_CUPS
GTK_PRINT_BACKENDS=file,cups,cloudprint
else
if HAVE_PAPI
GTK_PRINT_BACKENDS=file,papi,cloudprint
else
GTK_PRINT_BACKENDS=file,lpr,cloudprint
endif
endif
endif
else
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

if OS_WIN32
gtk_def = gtk.def
gtk_win32_symbols = -export-symbols $(srcdir)/gtk.def

gtk_win32_res = gtk-win32-res.o
gtk_win32_res_ldflag = -Wl,gtk-win32-res.o

gtk-win32-res.o : gtk-win32.rc libgtk3.manifest
	$(WINDRES) gtk-win32.rc $@

gtk.def: libgtk-3.la
	echo "LIBRARY libgtk-$(GTK_MAJOR_VERSION)-@LT_CURRENT_MINUS_AGE@" >$@
	echo "EXPORTS" >>$@
	objdump -p .libs/libgtk-$(GTK_MAJOR_VERSION)-@LT_CURRENT_MINUS_AGE@.dll | sed -n -e 's/\t\[\s\+\([0-9]\+\)\]\s\([A-Za-z_][A-Za-z0-9_]\)/\2/p' >>$@

install-def-file: gtk.def
	$(INSTALL) gtk.def $(DESTDIR)$(libdir)/gtk-win32-3.0.def
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
	$(top_builddir)/gtk/inspector/libgtkinspector.la \
	$(GMODULE_LIBS) \
	$(GTK_DEP_LIBS)
deps = \
	$(top_builddir)/gdk/libgdk-3.la	\
	$(top_builddir)/gtk/inspector/libgtkinspector.la

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

a11y_h_sources =				\
	a11y/gtkarrowaccessible.h		\
	a11y/gtkbooleancellaccessible.h		\
	a11y/gtkbuttonaccessible.h		\
	a11y/gtkcellaccessible.h		\
	a11y/gtkcellaccessibleparent.h		\
	a11y/gtkcheckmenuitemaccessible.h	\
	a11y/gtkcomboboxaccessible.h		\
	a11y/gtkcontaineraccessible.h		\
	a11y/gtkcontainercellaccessible.h	\
	a11y/gtkentryaccessible.h		\
	a11y/gtkexpanderaccessible.h		\
	a11y/gtkflowboxaccessible.h		\
	a11y/gtkflowboxchildaccessible.h	\
	a11y/gtkframeaccessible.h		\
	a11y/gtkiconviewaccessible.h		\
	a11y/gtkimageaccessible.h		\
	a11y/gtkimagecellaccessible.h		\
	a11y/gtklabelaccessible.h		\
	a11y/gtklevelbaraccessible.h 		\
	a11y/gtklinkbuttonaccessible.h		\
	a11y/gtklistboxaccessible.h		\
	a11y/gtklistboxrowaccessible.h		\
	a11y/gtklockbuttonaccessible.h		\
	a11y/gtkmenuaccessible.h		\
	a11y/gtkmenubuttonaccessible.h		\
	a11y/gtkmenuitemaccessible.h		\
	a11y/gtkmenushellaccessible.h		\
	a11y/gtknotebookaccessible.h		\
	a11y/gtknotebookpageaccessible.h	\
	a11y/gtkpanedaccessible.h		\
	a11y/gtkpopoveraccessible.h		\
	a11y/gtkprogressbaraccessible.h		\
	a11y/gtkradiobuttonaccessible.h		\
	a11y/gtkradiomenuitemaccessible.h	\
	a11y/gtkrangeaccessible.h		\
	a11y/gtkrenderercellaccessible.h	\
	a11y/gtkscaleaccessible.h		\
	a11y/gtkscalebuttonaccessible.h		\
	a11y/gtkscrolledwindowaccessible.h	\
	a11y/gtkspinbuttonaccessible.h		\
	a11y/gtkspinneraccessible.h		\
	a11y/gtkstatusbaraccessible.h		\
	a11y/gtkswitchaccessible.h		\
	a11y/gtktextcellaccessible.h		\
	a11y/gtktextviewaccessible.h		\
	a11y/gtktogglebuttonaccessible.h	\
	a11y/gtktoplevelaccessible.h		\
	a11y/gtktreeviewaccessible.h		\
	a11y/gtkwidgetaccessible.h		\
	a11y/gtkwindowaccessible.h

deprecated_h_sources = 			\
	deprecated/gtkactivatable.h	\
	deprecated/gtkaction.h		\
	deprecated/gtkactiongroup.h	\
	deprecated/gtkalignment.h	\
	deprecated/gtkarrow.h		\
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
	deprecated/gtkiconfactory.h	\
	deprecated/gtkimagemenuitem.h	\
	deprecated/gtkmisc.h		\
	deprecated/gtknumerableicon.h	\
	deprecated/gtkradioaction.h	\
	deprecated/gtkrc.h		\
	deprecated/gtkrecentaction.h	\
	deprecated/gtkstatusicon.h	\
	deprecated/gtkstock.h		\
	deprecated/gtkstyle.h		\
	deprecated/gtksymboliccolor.h	\
	deprecated/gtktable.h		\
	deprecated/gtktearoffmenuitem.h	\
	deprecated/gtkthemingengine.h	\
	deprecated/gtktoggleaction.h	\
	deprecated/gtkuimanager.h	\
	deprecated/gtkvbbox.h		\
	deprecated/gtkvbox.h		\
	deprecated/gtkvscale.h		\
	deprecated/gtkvscrollbar.h	\
	deprecated/gtkvseparator.h	\
	deprecated/gtkvpaned.h

a11y_private_h_sources =			\
	a11y/gtkaccessibility.h			\
	a11y/gtkaccessibilitymisc.h		\
	a11y/gtkaccessibilityutil.h		\
	a11y/gtkcellaccessibleprivate.h		\
	a11y/gtkcolorswatchaccessibleprivate.h	\
	a11y/gtkcontaineraccessibleprivate.h	\
	a11y/gtkiconviewaccessibleprivate.h	\
	a11y/gtklabelaccessibleprivate.h	\
	a11y/gtkflowboxaccessibleprivate.h	\
	a11y/gtklistboxaccessibleprivate.h	\
	a11y/gtklockbuttonaccessibleprivate.h	\
	a11y/gtktextviewaccessibleprivate.h	\
	a11y/gtktreeviewaccessibleprivate.h	\
	a11y/gtkwidgetaccessibleprivate.h

deprecated_private_h_sources =			\
	deprecated/gtkgradientprivate.h		\
	deprecated/gtknumerableiconprivate.h	\
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
	gtkactionbar.h		\
	gtkadjustment.h		\
	gtkappchooser.h		\
	gtkappchooserbutton.h	\
	gtkappchooserdialog.h	\
	gtkappchooserwidget.h	\
	gtkapplication.h	\
	gtkapplicationwindow.h	\
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
	gtkeventcontroller.h	\
	gtkexpander.h		\
	gtkfilechooser.h	\
	gtkfilechooserbutton.h	\
	gtkfilechooserdialog.h	\
	gtkfilechooserwidget.h	\
	gtkfilefilter.h		\
	gtkfixed.h		\
	gtkflowbox.h		\
	gtkfontbutton.h		\
	gtkfontchooser.h	\
	gtkfontchooserdialog.h  \
	gtkfontchooserwidget.h	\
	gtkframe.h		\
	gtkgesture.h		\
	gtkgesturedrag.h	\
	gtkgesturelongpress.h	\
	gtkgesturemultipress.h	\
	gtkgesturepan.h		\
	gtkgesturerotate.h	\
	gtkgesturesingle.h	\
	gtkgestureswipe.h	\
	gtkgesturezoom.h	\
	gtkgrid.h		\
	gtkheaderbar.h		\
	gtkicontheme.h		\
	gtkiconview.h		\
	gtkimage.h		\
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
	gtkmodules.h		\
	gtkmountoperation.h	\
	gtknotebook.h		\
	gtkoffscreenwindow.h	\
	gtkorientable.h		\
	gtkoverlay.h		\
	gtkpagesetup.h		\
	gtkpaned.h		\
	gtkpapersize.h		\
	gtkplacessidebar.h	\
	gtkplug.h		\
	gtkpopover.h		\
	gtkprintcontext.h	\
	gtkprintoperation.h	\
	gtkprintoperationpreview.h \
	gtkprintsettings.h	\
	gtkprogressbar.h	\
	gtkradiobutton.h	\
	gtkradiomenuitem.h	\
	gtkradiotoolbutton.h	\
	gtkrange.h		\
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
	gtksidebar.h		\
	gtksizegroup.h		\
	gtksizerequest.h	\
	gtksocket.h		\
	gtkspinbutton.h		\
	gtkspinner.h		\
	gtkstack.h		\
	gtkstackswitcher.h	\
	gtkstatusbar.h		\
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
	gtkviewport.h		\
	gtkvolumebutton.h	\
	gtkwidget.h		\
	gtkwidgetpath.h		\
	gtkwindow.h		\
	gtkwindowgroup.h

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

gtk_private_type_h_sources =	\
	gtkcsstypesprivate.h	\
	gtktexthandleprivate.h


# GTK+ header files that don't get installed
gtk_private_h_sources =		\
	gtkactionmuxer.h	\
	gtkactionobserver.h	\
	gtkactionobservable.h	\
	gtkadjustmentprivate.h	\
	gtkapplicationprivate.h	\
	gtkaccelgroupprivate.h	\
	gtkaccelmapprivate.h	\
	gtkactionhelper.h	\
	gtkallocatedbitmaskprivate.h	\
	gtkappchooserprivate.h	\
	gtkbindingsprivate.h	\
	gtkbitmaskprivate.h	\
	gtkbitmaskprivateimpl.h	\
	gtkbookmarksmanager.h	\
	gtkborderimageprivate.h \
	gtkboxprivate.h         \
	gtkbuilderprivate.h	\
	gtkbuttonprivate.h	\
	gtkcairoblurprivate.h	\
	gtkcellareaboxcontextprivate.h	\
	gtkclipboardprivate.h		\
	gtkcolorswatchprivate.h	\
	gtkcoloreditorprivate.h	\
	gtkcolorplaneprivate.h	\
	gtkcolorscaleprivate.h	\
	gtkcolorchooserprivate.h	\
	gtkcomboboxprivate.h	\
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
	gtkcssimageiconthemeprivate.h	\
	gtkcssimagelinearprivate.h	\
	gtkcssimageprivate.h	\
	gtkcssimagesurfaceprivate.h	\
	gtkcssimageurlprivate.h	\
	gtkcssimagescaledprivate.h	\
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
	gtkcsstransformvalueprivate.h	\
	gtkcsstransitionprivate.h	\
	gtkcsstypedvalueprivate.h	\
	gtkcssunsetvalueprivate.h	\
	gtkcssvalueprivate.h	\
	gtkcustompaperunixdialog.h \
	gtkdialogprivate.h 	\
	gtkentryprivate.h	\
	gtkeventcontrollerprivate.h	\
	gtkfilechooserembed.h	\
	gtkfilechooserentry.h	\
	gtkfilechooserprivate.h	\
	gtkfilechooserutils.h	\
	gtkfilesystem.h		\
	gtkfilesystemmodel.h	\
	gtkfontchooserprivate.h	\
	gtkfontchooserutils.h	\
	gtkgestureprivate.h	\
	gtkgesturedragprivate.h	\
	gtkgesturelongpressprivate.h	\
	gtkgesturemultipressprivate.h	\
	gtkgesturepanprivate.h	\
	gtkgesturerotateprivate.h	\
	gtkgesturesingleprivate.h	\
	gtkgestureswipeprivate.h	\
	gtkgesturezoomprivate.h	\
	gtkheaderbarprivate.h	\
	gtkhslaprivate.h	\
	gtkiconcache.h		\
	gtkiconhelperprivate.h  \
	gtkiconviewprivate.h	\
	gtkimageprivate.h	\
	gtkimmoduleprivate.h	\
	gtkimcontextsimpleseqs.h \
	gtkintl.h		\
	gtkkeyhash.h		\
	gtkkineticscrolling.h	\
	gtklabelprivate.h	\
	gtklockbuttonprivate.h	\
	gtkmagnifierprivate.h	\
	gtkmenubuttonprivate.h	\
	gtkmenuprivate.h	\
	gtkmenuitemprivate.h	\
	gtkmenusectionbox.h	\
	gtkmenushellprivate.h	\
	gtkmenutracker.h	\
	gtkmenutrackeritem.h	\
	gtkmnemonichash.h	\
	gtkmodelmenuitem.h	\
	gtkmodelbutton.h	\
	gtkmodifierstyle.h	\
	gtkmodulesprivate.h	\
	gtkmountoperationprivate.h \
	gtkorientableprivate.h	\
	gtkpango.h		\
	gtkpathbar.h		\
	gtkpopoverprivate.h	\
	gtkprintoperation-private.h \
	gtkprintutils.h		\
	gtkprivate.h		\
	gtkpixelcacheprivate.h	\
	gtkquery.h		\
	gtkrangeprivate.h	\
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
	deprecated/gtkthemingengineprivate.h \
	gtktrashmonitor.h	\
	gtktoolbarprivate.h	\
	gtktoolpaletteprivate.h	\
	gtktreedatalist.h	\
	gtktreeprivate.h	\
	gtkwidgetprivate.h	\
	gtkwin32themeprivate.h	\
	gtkwindowprivate.h	\
	gtktreemenu.h		\
	$(gtk_private_type_h_sources) \
	$(gtk_clipboard_dnd_h_sources)

a11y_c_sources =				\
	a11y/gtkaccessibility.c			\
	a11y/gtkaccessibilitymisc.c 		\
	a11y/gtkaccessibilityutil.c		\
	a11y/gtkarrowaccessible.c		\
	a11y/gtkbooleancellaccessible.c		\
	a11y/gtkbuttonaccessible.c		\
	a11y/gtkcellaccessible.c		\
	a11y/gtkcellaccessibleparent.c		\
	a11y/gtkcheckmenuitemaccessible.c	\
	a11y/gtkcolorswatchaccessible.c		\
	a11y/gtkcomboboxaccessible.c		\
	a11y/gtkcontaineraccessible.c		\
	a11y/gtkcontainercellaccessible.c	\
	a11y/gtkentryaccessible.c		\
	a11y/gtkexpanderaccessible.c		\
	a11y/gtkflowboxaccessible.c		\
	a11y/gtkflowboxchildaccessible.c	\
	a11y/gtkframeaccessible.c		\
	a11y/gtkiconviewaccessible.c		\
	a11y/gtkimageaccessible.c		\
	a11y/gtkimagecellaccessible.c		\
	a11y/gtklabelaccessible.c		\
	a11y/gtklevelbaraccessible.c		\
	a11y/gtklinkbuttonaccessible.c		\
	a11y/gtklistboxaccessible.c		\
	a11y/gtklistboxrowaccessible.c		\
	a11y/gtklockbuttonaccessible.c		\
	a11y/gtkmenuaccessible.c		\
	a11y/gtkmenubuttonaccessible.c		\
	a11y/gtkmenushellaccessible.c		\
	a11y/gtkmenuitemaccessible.c		\
	a11y/gtknotebookaccessible.c		\
	a11y/gtknotebookpageaccessible.c	\
	a11y/gtkpanedaccessible.c		\
	a11y/gtkpopoveraccessible.c		\
	a11y/gtkprogressbaraccessible.c		\
	a11y/gtkradiobuttonaccessible.c		\
	a11y/gtkradiomenuitemaccessible.c	\
	a11y/gtkrangeaccessible.c		\
	a11y/gtkrenderercellaccessible.c	\
	a11y/gtkscaleaccessible.c		\
	a11y/gtkscalebuttonaccessible.c		\
	a11y/gtkscrolledwindowaccessible.c	\
	a11y/gtkspinbuttonaccessible.c		\
	a11y/gtkspinneraccessible.c		\
	a11y/gtkstatusbaraccessible.c		\
	a11y/gtkswitchaccessible.c		\
	a11y/gtktextcellaccessible.c		\
	a11y/gtktextviewaccessible.c		\
	a11y/gtktogglebuttonaccessible.c	\
	a11y/gtktoplevelaccessible.c		\
	a11y/gtktreeviewaccessible.c		\
	a11y/gtkwidgetaccessible.c		\
	a11y/gtkwindowaccessible.c

# GTK+ C sources to build the library from
deprecated_c_sources = 			\
	deprecated/gtkactivatable.c	\
	deprecated/gtkaction.c		\
	deprecated/gtkactiongroup.c	\
	deprecated/gtkalignment.c	\
	deprecated/gtkarrow.c		\
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
	deprecated/gtkiconfactory.c	\
	deprecated/gtkimagemenuitem.c	\
	deprecated/gtkmisc.c		\
	deprecated/gtknumerableicon.c	\
	deprecated/gtkradioaction.c	\
	deprecated/gtkrc.c		\
	deprecated/gtkrecentaction.c	\
	deprecated/gtkstatusicon.c	\
	deprecated/gtkstock.c		\
	deprecated/gtkstyle.c		\
	deprecated/gtksymboliccolor.c	\
	deprecated/gtktable.c		\
	deprecated/gtktearoffmenuitem.c	\
	deprecated/gtkthemingengine.c	\
	deprecated/gtktoggleaction.c	\
	deprecated/gtkuimanager.c	\
	deprecated/gtkvbbox.c		\
	deprecated/gtkvbox.c		\
	deprecated/gtkvscale.c		\
	deprecated/gtkvscrollbar.c	\
	deprecated/gtkvseparator.c	\
	deprecated/gtkvpaned.c

gtk_base_c_sources = 		\
	$(a11y_c_sources)	\
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
	gtkactionbar.c		\
	gtkactionhelper.c	\
	gtkadjustment.c		\
	gtkallocatedbitmask.c	\
	gtkappchooser.c		\
	gtkappchooserwidget.c	\
	gtkappchooserbutton.c	\
	gtkappchooserdialog.c	\
	gtkapplication.c	\
	gtkapplicationimpl.c	\
	gtkapplicationwindow.c	\
	gtkaspectframe.c	\
	gtkassistant.c		\
	gtkbbox.c		\
	gtkbin.c		\
	gtkbindings.c		\
	gtkbookmarksmanager.c	\
	gtkborder.c		\
	gtkborderimage.c	\
	gtkbox.c		\
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
	gtkcssimageicontheme.c	\
	gtkcssimagelinear.c	\
	gtkcssimagesurface.c	\
	gtkcssimageurl.c	\
	gtkcssimagescaled.c	\
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
	gtkcsstransformvalue.c	\
	gtkcsstransition.c	\
	gtkcsstypedvalue.c	\
	gtkcssunsetvalue.c	\
	gtkcssvalue.c		\
	gtkcsstypes.c		\
	gtkdialog.c		\
	gtkdrawingarea.c	\
	gtkeditable.c		\
	gtkentry.c		\
	gtkentrybuffer.c	\
	gtkentrycompletion.c	\
	gtkeventbox.c		\
	gtkeventcontroller.c	\
	gtkexpander.c		\
	gtkfilechooser.c	\
	gtkfilechooserbutton.c	\
	gtkfilechooserdialog.c	\
	gtkfilechooserembed.c	\
	gtkfilechooserentry.c	\
	gtkfilechooserutils.c	\
	gtkfilechooserwidget.c	\
	gtkfilefilter.c		\
	gtkfilesystem.c		\
	gtkfilesystemmodel.c	\
	gtkfixed.c		\
	gtkflowbox.c		\
	gtkfontbutton.c		\
	gtkfontchooser.c	\
	gtkfontchooserdialog.c	\
	gtkfontchooserutils.c	\
	gtkfontchooserwidget.c	\
	gtkframe.c		\
	gtkgladecatalog.c	\
	gtkgesture.c		\
	gtkgesturedrag.c	\
	gtkgesturelongpress.c	\
	gtkgesturemultipress.c	\
	gtkgesturepan.c		\
	gtkgesturerotate.c	\
	gtkgesturesingle.c	\
	gtkgestureswipe.c	\
	gtkgesturezoom.c	\
	gtkgrid.c		\
	gtkheaderbar.c		\
	gtkhsla.c		\
	gtkiconcache.c		\
	gtkiconcachevalidator.c	\
	gtkiconhelper.c		\
	gtkicontheme.c		\
	gtkiconview.c		\
	gtkimage.c		\
	gtkimcontext.c		\
	gtkimcontextsimple.c	\
	gtkimmodule.c		\
	gtkimmulticontext.c	\
	gtkinfobar.c		\
	gtkinvisible.c		\
	gtkkeyhash.c		\
	gtkkineticscrolling.c	\
	gtklabel.c		\
	gtklayout.c		\
	gtklevelbar.c		\
	gtklinkbutton.c		\
	gtklistbox.c		\
	gtkliststore.c		\
	gtklockbutton.c		\
	gtkmain.c		\
	gtkmagnifier.c		\
	gtkmarshalers.c		\
	gtkmenu.c		\
	gtkmenubar.c		\
	gtkmenubutton.c		\
	gtkmenuitem.c		\
	gtkmenusectionbox.c	\
	gtkmenushell.c		\
	gtkmenutracker.c	\
	gtkmenutrackeritem.c	\
	gtkmenutoolbutton.c	\
	gtkmessagedialog.c	\
	gtkmnemonichash.c	\
	gtkmodelmenuitem.c	\
	gtkmodelbutton.c	\
	gtkmodifierstyle.c	\
	gtkmodules.c		\
	gtkmountoperation.c	\
	gtknotebook.c		\
	gtkoffscreenwindow.c	\
	gtkorientable.c		\
	gtkoverlay.c		\
	gtkpagesetup.c		\
	gtkpaned.c		\
	gtkpango.c		\
	gtkpapersize.c		\
	gtkpathbar.c		\
	gtkplacessidebar.c	\
	gtkprintcontext.c	\
	gtkprintoperation.c	\
	gtkprintoperationpreview.c \
	gtkprintsettings.c	\
	gtkprintutils.c		\
	gtkprivate.c		\
	gtkprivatetypebuiltins.c \
	gtkprogressbar.c	\
	gtkpixelcache.c		\
	gtkpopover.c		\
	gtkradiobutton.c	\
	gtkradiomenuitem.c	\
	gtkradiotoolbutton.c	\
	gtkrange.c		\
	gtkrbtree.c		\
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
	gtksidebar.c		\
	gtkspinbutton.c		\
	gtkspinner.c		\
	gtkstack.c		\
	gtkstackswitcher.c	\
	gtkstatusbar.c		\
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
	gtkvolumebutton.c	\
	gtkviewport.c		\
	gtkwidget.c		\
	gtkwidgetpath.c		\
	gtkwindow.c		\
	gtkwindowgroup.c	\
	gtkwin32theme.c		\
	$(gtk_clipboard_dnd_c_sources)

nodist_gtk_c_sources =
gtk_c_sources = $(gtk_base_c_sources)
gtk_all_c_sources = $(gtk_base_c_sources)

gtk_dbus_built_sources = gtkdbusgenerated.c gtkdbusgenerated.h

$(gtk_dbus_built_sources) : Makefile.am gtkdbusinterfaces.xml
	$(AM_V_GEN) gdbus-codegen \
	--interface-prefix org.Gtk. \
	--c-namespace _Gtk \
	--generate-c-code gtkdbusgenerated \
	$(srcdir)/gtkdbusinterfaces.xml

nodist_gtk_c_sources += $(gtk_dbus_built_sources)
gtk_all_c_sources += $(gtk_dbus_built_sources)

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
	gtksearchenginetracker.h
gtk_c_sources += $(gtk_os_unix_c_sources)
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
	deprecated/gtktrayicon-x11.c	\
	gtkapplication-x11.c	\
	gtkmountoperation-x11.c

gtk_use_wayland_c_sources = \
	gtkapplication-wayland.c

gtk_use_wayland_or_x11_c_sources = \
	gtkapplication-dbus.c

gtk_use_win32_c_sources = 	\
	gtkwin32embed.c		\
	gtkwin32embedwidget.c	\
	gtkmountoperation-stub.c
gtk_use_quartz_c_sources = 	\
	gtksearchenginequartz.c	\
	gtkmountoperation-stub.c \
	gtkapplication-quartz.c \
	gtkapplication-quartz-menu.c \
	gtkquartz.c
gtk_use_stub_c_sources = 	\
	gtkmountoperation-stub.c
gtk_all_c_sources += $(gtk_use_x11_c_sources) $(gtk_use_win32_c_sources) $(gtk_use_quartz_c_sources) $(gtk_use_stub_c_sources)
gtk_use_x11_private_h_sources = 	\
	gtkxembed.h		\
	deprecated/gtktrayicon.h	\
	xembed.h
if USE_X11
gtk_c_sources += $(gtk_use_x11_c_sources)
gtk_private_h_sources += $(gtk_use_x11_private_h_sources)
endif

if USE_WAYLAND
gtk_c_sources += $(gtk_use_wayland_c_sources)
endif

# pretty tricky way to write USE_WAYLAND || USE_X11...
if USE_WAYLAND
gtk_c_sources += $(gtk_use_wayland_or_x11_c_sources)
else
if USE_X11
gtk_c_sources += $(gtk_use_wayland_or_x11_c_sources)
else
endif
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
	gtkquartz.h
if USE_QUARTZ
gtk_c_sources += $(gtk_use_quartz_c_sources)
libgtk_3_la_CFLAGS = "-xobjective-c"
gtk_private_h_sources += $(gtk_use_quartz_private_h_sources)
endif

gtk_all_private_h_sources = \
	$(a11y_private_h_sources) \
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
# No wayland gtkdnd-wayland.c yet
gtk_clipboard_dnd_c_sources = 	\
	gtkclipboard.c \
	gtkdnd.c
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
	gtkresources.h				\
	gtkmarshalers.h				\
	gtkprivatetypebuiltins.h

gtk_built_sources =				\
	gtkresources.c				\
	gtk.gresource.xml			\
	gtktypebuiltins.c			\
	gtktypefuncs.c				\
	gtkmarshalers.c				\
	gtkprivatetypebuiltins.c		\
	${gtk_built_private_headers}		\
	${gtk_built_public_sources}

stamp_files =					\
	stamp-gtkmarshalers.h			\
	stamp-gtktypebuiltins.h			\
	stamp-gtkprivatetypebuiltins.h

# non-header sources (headers should be specified in the above variables)
# that don't serve as direct make target sources, i.e. they don't have
# their own .lo rules and don't get publically installed
gtk_extra_sources =				\
	paper_names.c				\
	paper_names_offsets.c			\
	gen-paper-names.c			\
	deprecated/gtkstatusicon-quartz.c	\
	gtkversion.h.in				\
	gtkmarshalers.list			\
	fallback-c89.c

#
# setup GTK+ sources and their dependencies
#
MAINTAINERCLEANFILES = \
	$(gtk_built_sources) 			\
	$(gtk_dbus_built_sources)		\
	$(stamp_files)

DISTCLEANFILES =

EXTRA_HEADERS =
EXTRA_DIST += $(gtk_all_private_h_sources) $(gtk_extra_sources)
EXTRA_DIST += $(gtk_built_sources)


pkgdatadir=$(datadir)/gtk-$(GTK_API_VERSION)
pkgdata_DATA = gtkbuilder.rng

EXTRA_DIST += gtkbuilder.rnc gtkbuilder.rng

COMPOSITE_TEMPLATES =				\
	resources/ui/gtkapplication-quartz.ui	\
	resources/ui/gtkaboutdialog.ui		\
	resources/ui/gtkactionbar.ui		\
	resources/ui/gtkappchooserdialog.ui	\
	resources/ui/gtkappchooserwidget.ui	\
	resources/ui/gtkassistant.ui		\
	resources/ui/gtkcolorchooserdialog.ui	\
	resources/ui/gtkcoloreditor.ui		\
	resources/ui/gtkdialog.ui		\
	resources/ui/gtkfilechooserbutton.ui	\
	resources/ui/gtkfilechooserwidget.ui	\
	resources/ui/gtkfilechooserdialog.ui	\
	resources/ui/gtkfontbutton.ui		\
	resources/ui/gtkfontchooserdialog.ui	\
	resources/ui/gtkfontchooserwidget.ui	\
	resources/ui/gtkinfobar.ui		\
	resources/ui/gtklockbutton.ui		\
	resources/ui/gtkmessagedialog.ui	\
	resources/ui/gtkpagesetupunixdialog.ui	\
	resources/ui/gtkpathbar.ui		\
	resources/ui/gtkprintunixdialog.ui	\
	resources/ui/gtkrecentchooserdefault.ui	\
	resources/ui/gtksearchbar.ui		\
	resources/ui/gtkscalebutton.ui		\
	resources/ui/gtkstatusbar.ui		\
	resources/ui/gtkvolumebutton.ui

template_headers = $(COMPOSITE_TEMPLATES:.ui=.ui.h)

%.ui.h: %.ui
	$(AM_V_GEN) mkdir -p $(dir $@) \
	&& $(top_builddir)/util/extract-strings$(EXEEXT) $< > $@

#
# rules to generate built sources
#
# setup autogeneration dependencies
gen_sources = xgen-gdef xgen-gtbh xgen-gtic xgen-gmh xgen-gmc xgen-gmlh xgen-gmlc xgen-gtfsrc.c xgen-gtf
CLEANFILES = $(gen_sources)

BUILT_SOURCES = \
	$(gtk_built_sources) \
	$(gtk_dbus_built_sources)

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
stamp-gtktypebuiltins.h: $(gtk_public_h_sources) $(a11y_h_sources) $(deprecated_h_sources) gtktypebuiltins.h.template
	$(AM_V_GEN) ( cd $(srcdir) && $(GLIB_MKENUMS) --template gtktypebuiltins.h.template \
		$(gtk_public_h_sources) $(a11y_h_sources) $(deprecated_h_sources) ) > xgen-gtbh \
	&& (cmp -s xgen-gtbh gtktypebuiltins.h || cp xgen-gtbh gtktypebuiltins.h ) \
	&& rm -f xgen-gtbh \
	&& echo timestamp > $(@F)
gtktypebuiltins.c: $(gtk_public_h_sources) $(a11y_h_sources) $(deprecated_h_sources) gtktypebuiltins.c.template
	$(AM_V_GEN) ( cd $(srcdir) && $(GLIB_MKENUMS) --template gtktypebuiltins.c.template \
		$(gtk_public_h_sources) $(a11y_h_sources) $(deprecated_h_sources) ) > xgen-gtbc \
	&& cp xgen-gtbc gtktypebuiltins.c  \
	&& rm -f xgen-gtbc

gtk.gresource.xml: Makefile.am
	$(AM_V_GEN) $(ECHO) "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" > $@; \
	$(ECHO) "<gresources>" >> $@; \
	$(ECHO) "  <gresource prefix=\"/org/gtk/libgtk\">" >> $@; \
	for f in $(srcdir)/resources/theme/Raleigh/*.css; do \
	  n=`basename $$f`; \
	  $(ECHO) "    <file alias=\"theme/Raleigh.css\">theme/Raleigh/$$n</file>" >> $@; \
	done; \
        $(ECHO) "    <file alias=\"theme/Adwaita.css\">theme/Adwaita/gtk.css</file>" >> $@; \
        $(ECHO) "    <file alias=\"theme/Adwaita-dark.css\">theme/Adwaita/gtk-dark.css</file>" >> $@; \
        $(ECHO) "    <file>theme/Adwaita/gtk-contained.css</file>" >> $@; \
        $(ECHO) "    <file>theme/Adwaita/gtk-contained-dark.css</file>" >> $@; \
        for f in $(srcdir)/resources/theme/Adwaita/assets/*; do \
          n=`basename $$f`; \
          $(ECHO) "    <file preprocess=\"to-pixdata\">theme/Adwaita/assets/$$n</file>" >> $@; \
        done; \
	if test "$(win32_theme)" = "yes"; then \
	  for f in $(srcdir)/resources/theme/win32/*.css; do \
	    n=`basename $$f`; \
	    $(ECHO) "    <file alias=\"theme/$$n\">theme/win32/$$n</file>" >> $@; \
	  done; \
	fi; \
	for f in $(srcdir)/resources/cursor/*.png; do \
	  n=`basename $$f`; \
	  $(ECHO) "    <file>cursor/$$n</file>" >> $@; \
	done; \
	for f in $(srcdir)/resources/ui/*.ui; do \
	  n=`basename $$f`; \
	  $(ECHO) "    <file compressed=\"true\">ui/$$n</file>" >> $@; \
	done; \
	for s in 16x16 22x22 24x24 32x32; do \
	  for f in $(srcdir)/resources/icons/$$s/actions/*.png; do \
	    n=`basename $$f`; \
	    $(ECHO) "    <file>icons/$$s/actions/$$n</file>" >> $@; \
	  done; \
	done; \
	$(ECHO) "  </gresource>" >> $@; \
	$(ECHO) "</gresources>" >> $@;

win32_theme_sources = \
	resources/theme/win32/gtk-win32.css \
	resources/theme/win32/gtk-win32-base.css \
	resources/theme/win32/gtk-win32-classic.css \
	resources/theme/win32/gtk-win32-xp.css

adwaita_sources = \
	resources/theme/Adwaita/assets.txt \
	resources/theme/Adwaita/assets.svg \
	resources/theme/Adwaita/_colors-public.scss \
	resources/theme/Adwaita/_colors.scss \
	resources/theme/Adwaita/_common.scss \
	resources/theme/Adwaita/_drawing.scss \
	resources/theme/Adwaita/gtk-contained-dark.scss \
	resources/theme/Adwaita/gtk-contained.scss \
	resources/theme/Adwaita/parse-sass.sh \
	resources/theme/Adwaita/render-assets.sh \
	resources/theme/Adwaita/Gemfile \
	resources/theme/Adwaita/README \
	resources/theme/Adwaita/gtk-contained.css \
	resources/theme/Adwaita/gtk-contained-dark.css \
	resources/theme/Adwaita/gtk.css \
	resources/theme/Adwaita/gtk-dark.css

Raleigh_sources = \
	resources/theme/Raleigh/gtk-default.css

resource_files = $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir)/resources --generate-dependencies $(builddir)/gtk.gresource.xml)

gtkresources.h: gtk.gresource.xml
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $< \
		--target=$@ --sourcedir=$(srcdir)/resources --c-name _gtk --generate-header --manual-register
gtkresources.c: gtk.gresource.xml $(resource_files) $(template_headers)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $< \
		--target=$@ --sourcedir=$(srcdir)/resources --c-name _gtk --generate-source --manual-register

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


gtktypefuncs.c: stamp-gtktypebuiltins.h stamp-gtkprivatetypebuiltins.h $(top_srcdir)/gtk/*.h $(top_srcdir)/gtk/a11y/*.h $(top_srcdir)/gtk/deprecated/*.h $(top_srcdir)/gdk/*.h Makefile
	$(AM_V_GEN) (echo '#undef GTK_COMPILATION' && echo '#include <gtk/gtkx.h>') > xgen-gtfsrc.c && \
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

a11yincludedir = $(includedir)/gtk-3.0/gtk/a11y
a11yinclude_HEADERS= $(a11y_h_sources)

deprecatedincludedir = $(includedir)/gtk-3.0/gtk/deprecated
deprecatedinclude_HEADERS= $(deprecated_h_sources)

gtkunixprintincludedir = $(includedir)/gtk-3.0/unix-print/gtk
gtkunixprintinclude_HEADERS = $(gtk_unix_print_public_h_sources)

nodist_libgtk_3_la_SOURCES = $(nodist_gtk_c_sources)
libgtk_3_la_SOURCES = $(gtk_c_sources)
libgtk_3_la_LDFLAGS = $(libtool_opts)
libgtk_3_la_LIBADD = $(libadd)
libgtk_3_la_DEPENDENCIES = $(deps)

if USE_WIN32
win32_theme=yes
libgtk_3_la_LIBADD += -lole32 -lgdi32 -lcomdlg32 -lwinspool -lcomctl32
libgtk_3_la_LDFLAGS += -Wl,-luuid
libgtk_3_la_DEPENDENCIES += $(gtk_win32_res) $(deps)
libgtk_target_ldflags = $(gtk_win32_res_ldflag)
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
	for F in `echo $(gtk_base_c_sources) $(gtk_os_win32_c_sources) $(gtk_use_win32_c_sources) $(gtk_dbus_built_sources) | tr '/' '\\'`; do \
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
	  rm -f $(template_headers); \
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
    $(filter-out %private.h gtktextdisplay.h gtktextlayout.h gtkx.h, $(gtkinclude_HEADERS) $(a11yinclude_HEADERS) $(deprecatedinclude_HEADERS)) \
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
	gtk-launch \
	gtk-encode-symbolic-svg

if BUILD_ICON_CACHE
bin_PROGRAMS += gtk-update-icon-cache

if OS_WIN32

# Workaround for UAC silliness: programs with "update" in their name
# are believed to be installers and require elevated privileges to be
# used... Embed a manifest file into executable to tell Windows that
# gtk-update-icon-cache.exe doesn't require any special privileges.

GTK_UPDATE_ICON_CACHE_MANIFEST = gtk-update-icon-cache.exe.manifest
GTK_UPDATE_ICON_CACHE_RC = gtk-update-icon-cache.rc
GTK_UPDATE_ICON_CACHE_MANIFEST_OBJECT = gtk-update-icon-cache_manifest.o

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

$(GTK_UPDATE_ICON_CACHE_RC):
	(echo 'CREATEPROCESS_MANIFEST_RESOURCE_ID RT_MANIFEST '$(GTK_UPDATE_ICON_CACHE_MANIFEST)) >$@

$(GTK_UPDATE_ICON_CACHE_MANIFEST_OBJECT): $(GTK_UPDATE_ICON_CACHE_RC) $(GTK_UPDATE_ICON_CACHE_MANIFEST)
	$(WINDRES) --input $< --output $@ --output-format=coff

endif
endif

gtk_query_immodules_3_0_DEPENDENCIES = $(DEPS)
gtk_query_immodules_3_0_LDADD = $(LDADDS) $(GMODULE_LIBS)
gtk_query_immodules_3_0_SOURCES = queryimmodules.c

if BUILD_ICON_CACHE
gtk_update_icon_cache_LDADD = $(GDK_PIXBUF_LIBS) $(GTK_UPDATE_ICON_CACHE_MANIFEST_OBJECT)
gtk_update_icon_cache_DEPENDENCIES = $(GTK_UPDATE_ICON_CACHE_MANIFEST_OBJECT)
gtk_update_icon_cache_SOURCES = updateiconcache.c
endif

gtk_encode_symbolic_svg_LDADD = $(GDK_PIXBUF_LIBS) $(top_builddir)/gdk/libgdk-3.la $(GTK_DEP_LIBS)
gtk_encode_symbolic_svg_SOURCES = encodesymbolic.c


gtk_launch_LDADD = $(LDADDS)
gtk_launch_SOURCES = gtk-launch.c

.PHONY: files test test-debug

files:
	@files=`ls $(DISTFILES) 2> /dev/null `; for p in $$files; do \
	  echo $$p; \
	done

gsettings_SCHEMAS = \
	org.gtk.Settings.FileChooser.gschema.xml \
	org.gtk.Settings.ColorChooser.gschema.xml \
	org.gtk.Settings.Debug.gschema.xml

@GSETTINGS_RULES@

EXTRA_DIST +=                   \
	$(resource_files)	\
	$(adwaita_sources)	\
	$(Raleigh_sources)	\
	$(win32_theme_sources)	\
	$(gsettings_SCHEMAS)	\
	libgtk3.manifest.in	\
	gtk-win32.rc.in		\
	gtkwin32embed.h		\
	gtkwin32embedwidget.h	\
	gtkwin32embedwidget.c	\
	gtkprint-win32.h	\
	gtkprint-win32.c	\
	gtksearchenginequartz.h \
	gtkdbusinterfaces.xml	\
	gtk-keys.css.default	\
	gtk-keys.css.emacs	\
	gtk-keys.css.mac	\
	makefile.msc		\
	makefile.msc.in		\
	gtktypebuiltins.c.template \
	gtktypebuiltins.h.template \
	gtkprivatetypebuiltins.c.template \
	gtkprivatetypebuiltins.h.template

install-data-local:

-include $(top_srcdir)/git.mk
