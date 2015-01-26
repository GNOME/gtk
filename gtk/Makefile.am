AUTOMAKE_OPTIONS = subdir-objects

include $(top_srcdir)/Makefile.decl

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
	-DGTK_PRINT_BACKEND_ENABLE_UNSUPPORTED		\
	-I$(top_builddir)				\
	-I$(top_builddir)/gtk				\
	-I$(top_srcdir) 				\
	-I$(top_srcdir)/gdk				\
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

libadd = 				\
	$(top_builddir)/gdk/libgdk-3.la	\
	$(GMODULE_LIBS) 		\
	$(GTK_DEP_LIBS)
deps = 					\
	$(top_builddir)/gdk/libgdk-3.la

# libtool stuff: set version and export symbols for resolving
# since automake doesn't support conditionalized libsomething_la_LDFLAGS
# we use the general approach here
libtool_opts =							\
  $(GTK_LINK_FLAGS)						\
  -version-info $(LT_VERSION_INFO)				\
  -export-dynamic $(no_undefined)				\
  -rpath $(libdir)


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

include $(srcdir)/a11y/Makefile.inc
include $(srcdir)/deprecated/Makefile.inc
include $(srcdir)/inspector/Makefile.inc

# GTK+ header files for public installation (non-generated, or generated
# by configure)

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
	gtkglarea.h		\
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
	gtkmodelbutton.h	\
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
	gtkpopovermenu.h	\
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
	gtkrender.h		\
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
	$(inspector_h_sources)	\
	$(gtk_private_type_h_sources) \
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
	gtkcssanimatedstyleprivate.h	\
	gtkcssarrayvalueprivate.h	\
	gtkcssbgsizevalueprivate.h	\
	gtkcssbordervalueprivate.h	\
	gtkcsscolorvalueprivate.h	\
	gtkcsscornervalueprivate.h	\
	gtkcsscustompropertyprivate.h \
	gtkcsseasevalueprivate.h	\
	gtkcssenginevalueprivate.h	\
	gtkcssenumvalueprivate.h	\
	gtkcssiconthemevalueprivate.h	\
	gtkcssimagebuiltinprivate.h	\
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
	gtkcssnodedeclarationprivate.h	\
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
	gtkcssstaticstyleprivate.h	\
	gtkcssstringvalueprivate.h	\
	gtkcssstylefuncsprivate.h \
	gtkcssstyleprivate.h 	\
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
	gtkrenderbackgroundprivate.h \
	gtkrenderborderprivate.h \
	gtkrendericonprivate.h	\
	gtkrenderprivate.h	\
	gtkresources.h		\
	gtkroundedboxprivate.h	\
	gtkscaleprivate.h	\
	gtksearchengine.h	\
	gtksearchenginesimple.h	\
	gtksearchentryprivate.h \
	gtkselectionprivate.h	\
	gtksettingsprivate.h	\
	gtksizegroup-private.h	\
	gtksizerequestcacheprivate.h	\
	gtksocketprivate.h	\
	gtkstyleanimationprivate.h \
	gtkstylecascadeprivate.h \
	gtkstylecontextprivate.h \
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
	gtktrashmonitor.h	\
	gtktogglebuttonprivate.h \
	gtktoolbarprivate.h	\
	gtktoolpaletteprivate.h	\
	gtktooltipprivate.h	\
	gtktreedatalist.h	\
	gtktreeprivate.h	\
	gtkwidgetprivate.h	\
	gtkwin32themeprivate.h	\
	gtkwindowprivate.h	\
	gtktreemenu.h

# GTK+ C sources to build the library from
gtk_base_c_sources = 		\
	$(a11y_c_sources)	\
	$(deprecated_c_sources)	\
	$(inspector_c_sources)	\
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
	gtkcssanimatedstyle.c	\
	gtkcssarrayvalue.c	\
	gtkcssbgsizevalue.c	\
	gtkcssbordervalue.c	\
	gtkcsscolorvalue.c	\
	gtkcsscornervalue.c	\
	gtkcsscustomproperty.c	\
	gtkcsseasevalue.c	\
	gtkcssenumvalue.c	\
	gtkcssenginevalue.c	\
	gtkcssiconthemevalue.c	\
	gtkcssimage.c		\
	gtkcssimagebuiltin.c	\
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
	gtkcssnodedeclaration.c	\
	gtkcssnumbervalue.c	\
	gtkcssparser.c		\
	gtkcsspositionvalue.c	\
	gtkcssprovider.c	\
	gtkcssrepeatvalue.c	\
	gtkcssrgbavalue.c	\
	gtkcsssection.c 	\
	gtkcssselector.c	\
	gtkcssstringvalue.c	\
	gtkcssstyle.c		\
	gtkcssshadowsvalue.c	\
	gtkcssshadowvalue.c	\
	gtkcssshorthandproperty.c \
	gtkcssshorthandpropertyimpl.c \
	gtkcssstaticstyle.c	\
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
	gtkglarea.c		\
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
	gtkpopovermenu.c	\
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
	gtkrender.c		\
	gtkrenderbackground.c	\
	gtkrenderborder.c	\
	gtkrendericon.c		\
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
	gtkwin32theme.c

if USE_QUARTZ
gtk_base_c_sources += 		\
	gtkclipboard-quartz.c	\
	gtkdnd-quartz.c
else
gtk_base_c_sources += 		\
	gtkclipboard.c 		\
	gtkdnd.c
endif

nodist_gtk_c_sources =
gtk_c_sources = $(gtk_base_c_sources)

gtk_dbus_built_sources = gtkdbusgenerated.c gtkdbusgenerated.h

$(gtk_dbus_built_sources) : Makefile.am gtkdbusinterfaces.xml
	$(AM_V_GEN) gdbus-codegen \
	--interface-prefix org.Gtk. \
	--c-namespace _Gtk \
	--generate-c-code gtkdbusgenerated \
	$(srcdir)/gtkdbusinterfaces.xml

nodist_gtk_c_sources += $(gtk_dbus_built_sources)

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
	$(gtk_built_private_headers)		\
	$(gtk_built_public_sources)		\
	gtkresources.c				\
	gtk.gresource.xml			\
	gtktypebuiltins.c			\
	gtktypefuncs.c				\
	gtkmarshalers.c				\
	gtkprivatetypebuiltins.c

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

EXTRA_DIST += $(gtk_all_private_h_sources) $(gtk_extra_sources)
EXTRA_DIST += $(gtk_built_sources)


pkgdatadir = $(datadir)/gtk-$(GTK_API_VERSION)
pkgdata_DATA = gtkbuilder.rng

EXTRA_DIST += gtkbuilder.rnc gtkbuilder.rng

templates =				\
	$(inspector_templates)		\
	ui/gtkapplication-quartz.ui	\
	ui/gtkaboutdialog.ui		\
	ui/gtkactionbar.ui		\
	ui/gtkappchooserdialog.ui	\
	ui/gtkappchooserwidget.ui	\
	ui/gtkassistant.ui		\
	ui/gtkcolorchooserdialog.ui	\
	ui/gtkcoloreditor.ui		\
	ui/gtkdialog.ui		\
	ui/gtkfilechooserbutton.ui	\
	ui/gtkfilechooserwidget.ui	\
	ui/gtkfilechooserdialog.ui	\
	ui/gtkfontbutton.ui		\
	ui/gtkfontchooserdialog.ui	\
	ui/gtkfontchooserwidget.ui	\
	ui/gtkinfobar.ui		\
	ui/gtklockbutton.ui		\
	ui/gtkmessagedialog.ui	\
	ui/gtkpagesetupunixdialog.ui	\
	ui/gtkpathbar.ui		\
	ui/gtkprintunixdialog.ui	\
	ui/gtkrecentchooserdefault.ui	\
	ui/gtksearchbar.ui		\
	ui/gtkscalebutton.ui		\
	ui/gtkstatusbar.ui		\
	ui/gtkvolumebutton.ui

template_headers = $(templates:.ui=.ui.h)

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
	$(AM_V_GEN) echo "<?xml version='1.0' encoding='UTF-8'?>" > $@; \
	echo "<gresources>" >> $@; \
	echo "  <gresource prefix='/org/gtk/libgtk'>" >> $@; \
	for f in $(srcdir)/theme/Raleigh/*.css; do \
	  n=`basename $$f`; \
	  echo "    <file alias='theme/Raleigh.css'>theme/Raleigh/$$n</file>" >> $@; \
	done; \
        echo "    <file alias='theme/Adwaita.css'>theme/Adwaita/gtk.css</file>" >> $@; \
        echo "    <file alias='theme/Adwaita-dark.css'>theme/Adwaita/gtk-dark.css</file>" >> $@; \
        echo "    <file>theme/Adwaita/gtk-contained.css</file>" >> $@; \
        echo "    <file>theme/Adwaita/gtk-contained-dark.css</file>" >> $@; \
        for f in $(srcdir)/theme/Adwaita/assets/*; do \
          n=`basename $$f`; \
          echo "    <file preprocess='to-pixdata'>theme/Adwaita/assets/$$n</file>" >> $@; \
        done; \
	echo "    <file alias='theme/HighContrast.css'>theme/HighContrast/gtk.css</file>" >> $@; \
	if test "$(win32_theme)" = "yes"; then \
	  for f in $(srcdir)/theme/win32/*.css; do \
	    n=`basename $$f`; \
	    echo "    <file alias='theme/$$n'>theme/win32/$$n</file>" >> $@; \
	  done; \
	fi; \
	for f in $(srcdir)/cursor/*.png; do \
	  n=`basename $$f`; \
	  echo "    <file>cursor/$$n</file>" >> $@; \
	done; \
	for f in $(srcdir)/ui/*.ui; do \
	  n=`basename $$f`; \
	  echo "    <file compressed='true'>ui/$$n</file>" >> $@; \
	done; \
	for s in 16x16 22x22 24x24 32x32; do \
	  for f in $(srcdir)/icons/$$s/actions/*.png; do \
	    n=`basename $$f`; \
	    echo "    <file>icons/$$s/actions/$$n</file>" >> $@; \
	  done; \
	done; \
	for f in $(srcdir)/inspector/*.ui; do \
	  n=`basename $$f`; \
	  echo "    <file compressed='true'>inspector/$$n</file>" >> $@; \
	done; \
	echo "    <file>inspector/logo.png</file>" >> $@; \
	echo "  </gresource>" >> $@; \
	echo "</gresources>" >> $@;

theme_sources = 				\
	theme/win32/gtk-win32.css 		\
	theme/win32/gtk-win32-base.css 		\
	theme/win32/gtk-win32-classic.css 	\
	theme/win32/gtk-win32-xp.css 		\
	theme/Adwaita/assets.txt 		\
	theme/Adwaita/assets.svg 		\
	theme/Adwaita/_colors-public.scss 	\
	theme/Adwaita/_colors.scss 		\
	theme/Adwaita/_common.scss 		\
	theme/Adwaita/_drawing.scss 		\
	theme/Adwaita/gtk-contained-dark.scss 	\
	theme/Adwaita/gtk-contained.scss 	\
	theme/Adwaita/parse-sass.sh 		\
	theme/Adwaita/render-assets.sh 		\
	theme/Adwaita/Gemfile 			\
	theme/Adwaita/README 			\
	theme/Adwaita/gtk-contained.css 	\
	theme/Adwaita/gtk-contained-dark.css 	\
	theme/Adwaita/gtk.css 			\
	theme/Adwaita/gtk-dark.css 		\
	theme/HighContrast/_colors.scss 	\
	theme/HighContrast/_common.scss 	\
	theme/HighContrast/_drawing.scss 	\
	theme/HighContrast/Gemfile 		\
	theme/HighContrast/gtk.css 		\
	theme/HighContrast/gtk.scss 		\
	theme/HighContrast/parse-sass.sh 	\
	theme/Raleigh/gtk-default.css

resource_files = $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(builddir)/gtk.gresource.xml)

gtkresources.h: gtk.gresource.xml
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $< \
		--target=$@ --sourcedir=$(srcdir) --c-name _gtk --generate-header --manual-register
gtkresources.c: gtk.gresource.xml $(resource_files) $(template_headers)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $< \
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
win32_theme = yes
libgtk_3_la_LIBADD += -lole32 -lgdi32 -lcomdlg32 -lwinspool -lcomctl32
libgtk_3_la_LDFLAGS += -Wl,-luuid $(gtk_win32_res_ldflag)
libgtk_3_la_DEPENDENCIES += $(gtk_win32_res)
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
	rm -f $(DESTDIR)$(datadir)/themes/Default/gtk-3.0/gtk-keys.css
	rm -f $(DESTDIR)$(datadir)/themes/Emacs/gtk-3.0/gtk-keys.css

# if srcdir!=builddir, clean out maintainer-clean files from builddir
# this allows dist to pass.
distclean-local:
	if test $(srcdir) != .; then \
	  rm -f $(MAINTAINERCLEANFILES); \
	fi

if HAVE_INTROSPECTION
introspection_files = \
    $(filter-out %private.h gtktextdisplay.h gtktextlayout.h gtkx.h, $(gtkinclude_HEADERS) $(a11yinclude_HEADERS) $(deprecatedinclude_HEADERS)) \
    $(gtk_base_c_sources) \
    gtkprintoperation-unix.c \
    gtktypebuiltins.h \
    gtktypebuiltins.c

if USE_X11
introspection_files += \
    gtksocket.c \
    gtkplug.c
endif

include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_SCANNER_ENV = CC="$(CC)"
INTROSPECTION_COMPILER_ARGS = \
	--includedir=$(srcdir) \
	--includedir=. \
	--includedir=../gdk

Gtk-3.0.gir: $(INTROSPECTION_SCANNER) libgtk-3.la $(top_builddir)/gdk/Gdk-3.0.gir Makefile
Gtk_3_0_gir_SCANNERFLAGS = 			\
	--warn-all				\
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
Gtk_3_0_gir_FILES = $(introspection_files)
Gtk_3_0_gir_LIBS = libgtk-3.la $(top_builddir)/gdk/libgdk-3.la
Gtk_3_0_gir_EXPORT_PACKAGES = gtk+-3.0
INTROSPECTION_GIRS = Gtk-3.0.gir

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
	gtk-update-icon-cache \
	gtk-encode-symbolic-svg \
	gtk-launch

gtk_query_immodules_3_0_SOURCES = queryimmodules.c
gtk_query_immodules_3_0_LDADD = 		\
	libgtk-3.la				\
	$(top_builddir)/gdk/libgdk-3.la		\
	$(GMODULE_LIBS)				\
	$(GTK_DEP_LIBS)

gtk_update_icon_cache_SOURCES = updateiconcache.c
gtk_update_icon_cache_LDADD = $(GDK_PIXBUF_LIBS)

gtk_encode_symbolic_svg_SOURCES = encodesymbolic.c
gtk_encode_symbolic_svg_LDADD =			\
	$(GDK_PIXBUF_LIBS)			\
	$(top_builddir)/gdk/libgdk-3.la		\
	$(GTK_DEP_LIBS)

gtk_launch_SOURCES = gtk-launch.c
gtk_launch_LDADD =				\
	libgtk-3.la				\
	$(top_builddir)/gdk/libgdk-3.la		\
	$(GTK_DEP_LIBS)

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

gtk_update_icon_cache_LDADD += $(GTK_UPDATE_ICON_CACHE_MANIFEST_OBJECT)

endif

.PHONY: files

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
	$(theme_sources)	\
	$(gsettings_SCHEMAS)	\
	$(template_headers)	\
	a11y/Makefile.inc	\
	deprecated/Makefile.inc	\
	inspector/Makefile.inc	\
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

-include $(top_srcdir)/git.mk
