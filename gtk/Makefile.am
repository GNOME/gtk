AUTOMAKE_OPTIONS = subdir-objects

include $(top_srcdir)/Makefile.decl

GTK_BASE_CFLAGS_DEFINES =				\
	-DG_LOG_USE_STRUCTURED=1			\
	-DGTK_VERSION=\"$(GTK_VERSION)\"		\
	-DGTK_BINARY_VERSION=\"$(GTK_BINARY_VERSION)\"	\
	-DGTK_COMPILATION				\
	-DGTK_PRINT_BACKEND_ENABLE_UNSUPPORTED

GTK_PLAT_CFLAGS_DEFINES =				\
	-DGTK_LIBDIR=\"$(libdir)\"			\
	-DGTK_LOCALEDIR=\"$(localedir)\"		\
	-DGTK_DATADIR=\"$(datadir)\"			\
	-DGTK_DATA_PREFIX=\"$(prefix)\"			\
	-DGTK_SYSCONFDIR=\"$(sysconfdir)\"		\
	-DGTK_HOST=\"$(host)\"				\
	-DGTK_PRINT_BACKENDS=\"$(GTK_PRINT_BACKENDS)\"

AM_CPPFLAGS =						\
	-DG_LOG_DOMAIN=\"Gtk\"				\
	$(GTK_BASE_CFLAGS_DEFINES)			\
	$(GTK_PLAT_CFLAGS_DEFINES)			\
	-DX11_DATA_PREFIX=\"$(X11_PREFIX)\"		\
	-DISO_CODES_PREFIX=\"$(ISO_CODES_PREFIX)\"	\
	-I$(top_builddir)				\
	-I$(top_builddir)/gtk				\
	-I$(top_srcdir) 				\
	-I$(top_srcdir)/gdk				\
	$(GMODULE_CFLAGS)				\
	$(GTK_DEBUG_FLAGS)				\
	$(GTK_DEP_CFLAGS)				\
	$(INCLUDED_IMMODULE_DEFINE)

if OS_WIN32
gtk_def = gtk.def
gtk_win32_symbols = -export-symbols $(srcdir)/gtk.def

gtk_win32_res = gtk-win32-res.o
gtk_win32_res_ldflag = -Wl,gtk-win32-res.o

gtk-win32-res.o : gtk-win32.rc libgtk3.manifest
	$(WINDRES) gtk-win32.rc $@

gtk-win32.rc: gtk-win32.rc.body
	cat $< >>$@
	echo "ISOLATIONAWARE_MANIFEST_RESOURCE_ID RT_MANIFEST libgtk3.manifest" >>$@

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

gtk-win32-$(GTK_API_VERSION).lib: libgtk-3.la gtk.def
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
include $(srcdir)/gtk-mkenums-headers.mak

# GTK+ header files for public installation (non-generated, or generated
# by configure)

gtk_public_h_sources = $(GTK_PUB_HDRS)

if OS_UNIX
gtk_unix_print_public_h_sources = 	\
	gtkpagesetupunixdialog.h	\
	gtkprintunixdialog.h		\
	gtkprinter.h			\
	gtkprintjob.h			\
	gtkunixprint-autocleanups.h	\
	gtkunixprint.h
endif

# Installed header files without compatibility guarantees
# that are not included in gtk/gtk.h
gtk_semi_private_h_sources = \
	gtktextlayout.h

gtk_private_type_h_sources = $(GTK_PRIVATE_TYPE_HDRS)

# GTK+ header files that don't get installed
gtk_private_h_sources =		\
	$(inspector_h_sources)	\
	$(gtk_private_type_h_sources) \
	gtkactionmuxer.h	\
	gtkactionobserver.h	\
	gtkactionobservable.h	\
	gtkadjustmentprivate.h	\
	gtkapplicationaccelsprivate.h	\
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
	gtkboxgadgetprivate.h	\
	gtkbuilderprivate.h	\
	gtkbuiltiniconprivate.h	\
	gtkbuttonprivate.h	\
	gtkcairoblurprivate.h	\
	gtkcellareaboxcontextprivate.h	\
	gtkcheckbuttonprivate.h	\
	gtkcheckmenuitemprivate.h	\
	gtkclipboardprivate.h		\
	gtkcolorswatchprivate.h	\
	gtkcoloreditorprivate.h	\
	gtkcolorpickerprivate.h	\
	gtkcolorpickerkwinprivate.h	\
	gtkcolorpickerportalprivate.h	\
	gtkcolorpickershellprivate.h	\
	gtkcolorplaneprivate.h	\
	gtkcolorscaleprivate.h	\
	gtkcolorchooserprivate.h	\
	gtkcomboboxprivate.h	\
	gtkcomposetable.h	\
	gtkcontainerprivate.h   \
	gtkcssanimationprivate.h	\
	gtkcssanimatedstyleprivate.h	\
	gtkcssarrayvalueprivate.h	\
	gtkcssbgsizevalueprivate.h	\
	gtkcssbordervalueprivate.h	\
	gtkcsscalcvalueprivate.h	\
	gtkcsscolorvalueprivate.h	\
	gtkcsscornervalueprivate.h	\
	gtkcsscustomgadgetprivate.h	\
	gtkcsscustompropertyprivate.h 	\
	gtkcssdimensionvalueprivate.h	\
	gtkcsseasevalueprivate.h	\
	gtkcssenginevalueprivate.h	\
	gtkcssenumvalueprivate.h	\
	gtkcssgadgetprivate.h		\
	gtkcssiconthemevalueprivate.h	\
	gtkcssimagebuiltinprivate.h	\
	gtkcssimagecrossfadeprivate.h	\
	gtkcssimagefallbackprivate.h	\
	gtkcssimagegradientprivate.h	\
	gtkcssimageiconthemeprivate.h	\
	gtkcssimagelinearprivate.h	\
	gtkcssimageradialprivate.h	\
	gtkcssimageprivate.h	\
	gtkcssimagesurfaceprivate.h	\
	gtkcssimageurlprivate.h	\
	gtkcssimagerecolorprivate.h	\
	gtkcssimagescaledprivate.h	\
	gtkcssimagevalueprivate.h	\
	gtkcssimagewin32private.h	\
	gtkcssinheritvalueprivate.h	\
	gtkcssinitialvalueprivate.h	\
	gtkcsskeyframesprivate.h	\
	gtkcsslookupprivate.h	\
	gtkcssmatcherprivate.h	\
	gtkcssnodeprivate.h		\
	gtkcssnodedeclarationprivate.h	\
	gtkcssnodestylecacheprivate.h	\
	gtkcssnumbervalueprivate.h	\
	gtkcsspalettevalueprivate.h	\
	gtkcssparserprivate.h	\
	gtkcsspathnodeprivate.h	\
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
	gtkcssstylechangeprivate.h 	\
	gtkcssstyleprivate.h 	\
	gtkcssstylepropertyprivate.h \
	gtkcsstransformvalueprivate.h	\
	gtkcsstransientnodeprivate.h	\
	gtkcsstransitionprivate.h	\
	gtkcsstypedvalueprivate.h	\
	gtkcssunsetvalueprivate.h	\
	gtkcssvalueprivate.h	\
	gtkcsswin32sizevalueprivate.h   \
	gtkcsswidgetnodeprivate.h	\
	gtkcustompaperunixdialog.h \
	gtkdialogprivate.h 	\
	gtkdndprivate.h		\
	gtkemojichooser.h	\
	gtkemojicompletion.h	\
	gtkentryprivate.h	\
	gtkeventcontrollerprivate.h	\
	gtkfilechooserembed.h	\
	gtkfilechooserentry.h	\
	gtkfilechooserprivate.h	\
	gtkfilechoosernativeprivate.h	\
	gtkfilechooserwidgetprivate.h	\
	gtkfilechooserutils.h	\
	gtkfilefilterprivate.h	\
	gtkfilesystem.h		\
	gtkfilesystemmodel.h	\
	gtkfontchooserprivate.h	\
	gtkfontchooserutils.h	\
	gtkfontchooserwidgetprivate.h \
	gtkgestureprivate.h	\
	gtkgesturedragprivate.h	\
	gtkgesturelongpressprivate.h	\
	gtkgesturemultipressprivate.h	\
	gtkgesturepanprivate.h	\
	gtkgesturerotateprivate.h	\
	gtkgesturesingleprivate.h	\
	gtkgesturestylusprivate.h	\
	gtkgestureswipeprivate.h	\
	gtkgesturezoomprivate.h	\
	gtkheaderbarprivate.h	\
	gtkhslaprivate.h	\
	gtkiconcache.h		\
	gtkiconhelperprivate.h  \
	gtkiconprivate.h	\
	gtkiconthemeprivate.h  \
	gtkiconviewprivate.h	\
	gtkimagedefinitionprivate.h	\
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
	gtknativedialogprivate.h \
	gtkorientableprivate.h	\
	gtkpango.h		\
	gtkpathbar.h		\
	gtkplacessidebarprivate.h	\
	gtkplacesviewprivate.h	\
	gtkplacesviewrowprivate.h	\
	gtkpopoverprivate.h	\
	gtkprintoperation-private.h \
	gtkprintutils.h		\
	gtkprivate.h		\
	gtkpixelcacheprivate.h	\
	gtkprogresstrackerprivate.h	\
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
	gtksearchengine.h	\
	gtksearchenginesimple.h	\
	gtksearchenginemodel.h	\
	gtksearchentryprivate.h \
	gtkselectionprivate.h	\
	gtksettingsprivate.h	\
	gtkshortcutswindowprivate.h	\
	gtkshortcutsshortcutprivate.h	\
	gtksidebarrowprivate.h	\
	gtksizegroup-private.h	\
	gtksizerequestcacheprivate.h	\
	gtksocketprivate.h	\
	gtkstyleanimationprivate.h \
	gtkstylecascadeprivate.h \
	gtkstylecontextprivate.h \
	gtkstylepropertyprivate.h \
	gtkstyleproviderprivate.h \
	gtktextattributesprivate.h \
	gtktextbtree.h		\
	gtktextbufferprivate.h \
	gtktextbufferserialize.h \
	gtktextchildprivate.h	\
	gtktexthandleprivate.h	\
	gtktextiterprivate.h	\
	gtktextmarkprivate.h	\
	gtktextsegment.h	\
	gtktexttagprivate.h	\
	gtktexttagtableprivate.h	\
	gtktexttypes.h		\
	gtktextutil.h		\
	gtktrashmonitor.h	\
	gtktogglebuttonprivate.h \
	gtktoolbarprivate.h	\
	gtktoolpaletteprivate.h	\
	gtktooltipprivate.h	\
	gtktooltipwindowprivate.h \
	gtktreedatalist.h	\
	gtktreeprivate.h	\
	gtkutilsprivate.h	\
	gtkwidgetprivate.h	\
	gtkwidgetpathprivate.h	\
	gtkwin32drawprivate.h	\
	gtkwin32themeprivate.h	\
	gtkwindowprivate.h	\
	gtktreemenu.h		\
	gdkpixbufutilsprivate.h \
	open-type-layout.h	\
	language-names.h	\
	script-names.h

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
	gtksearchenginemodel.c	\
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
	gtkapplicationaccels.c	\
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
	gtkboxgadget.c		\
	gtkbuildable.c		\
	gtkbuilder.c		\
	gtkbuilderparser.c	\
	gtkbuilder-menus.c	\
	gtkbuiltinicon.c	\
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
	gtkcolorpicker.c	\
	gtkcolorpickerkwin.c	\
	gtkcolorpickerportal.c	\
	gtkcolorpickershell.c	\
	gtkcolorplane.c		\
	gtkcolorscale.c		\
	gtkcolorswatch.c	\
	gtkcolorutils.c		\
	gtkcombobox.c		\
	gtkcomboboxtext.c	\
	gtkcomposetable.c	\
	gtkcontainer.c		\
	gtkcssanimation.c	\
	gtkcssanimatedstyle.c	\
	gtkcssarrayvalue.c	\
	gtkcssbgsizevalue.c	\
	gtkcssbordervalue.c	\
	gtkcsscalcvalue.c	\
	gtkcsscolorvalue.c	\
	gtkcsscornervalue.c	\
	gtkcsscustomgadget.c	\
	gtkcsscustomproperty.c	\
	gtkcssdimensionvalue.c	\
	gtkcsseasevalue.c	\
	gtkcssenumvalue.c	\
	gtkcssenginevalue.c	\
	gtkcssgadget.c		\
	gtkcssiconthemevalue.c	\
	gtkcssimage.c		\
	gtkcssimagebuiltin.c	\
	gtkcssimagecrossfade.c	\
	gtkcssimagefallback.c	\
	gtkcssimagegradient.c	\
	gtkcssimageicontheme.c	\
	gtkcssimagelinear.c	\
	gtkcssimageradial.c	\
	gtkcssimagesurface.c	\
	gtkcssimageurl.c	\
	gtkcssimagerecolor.c	\
	gtkcssimagescaled.c	\
	gtkcssimagevalue.c	\
	gtkcssimagewin32.c	\
	gtkcssinheritvalue.c	\
	gtkcssinitialvalue.c	\
	gtkcsskeyframes.c	\
	gtkcsslookup.c		\
	gtkcssmatcher.c		\
	gtkcssnode.c		\
	gtkcssnodedeclaration.c	\
	gtkcssnodestylecache.c	\
	gtkcssnumbervalue.c	\
	gtkcsspalettevalue.c	\
	gtkcssparser.c		\
	gtkcsspathnode.c	\
	gtkcsspositionvalue.c	\
	gtkcssprovider.c	\
	gtkcssrepeatvalue.c	\
	gtkcssrgbavalue.c	\
	gtkcsssection.c 	\
	gtkcssselector.c	\
	gtkcssstringvalue.c	\
	gtkcssstyle.c		\
	gtkcssstylechange.c 	\
	gtkcssshadowsvalue.c	\
	gtkcssshadowvalue.c	\
	gtkcssshorthandproperty.c \
	gtkcssshorthandpropertyimpl.c \
	gtkcssstaticstyle.c	\
	gtkcssstylefuncs.c	\
	gtkcssstyleproperty.c	\
	gtkcssstylepropertyimpl.c \
	gtkcsstransformvalue.c	\
	gtkcsstransientnode.c	\
	gtkcsstransition.c	\
	gtkcsstypedvalue.c	\
	gtkcssunsetvalue.c	\
	gtkcsstypes.c		\
	gtkcssvalue.c		\
	gtkcsswidgetnode.c	\
        gtkcsswin32sizevalue.c  \
	gtkdialog.c		\
	gtkdragsource.c		\
	gtkdrawingarea.c	\
	gtkeditable.c		\
	gtkemojichooser.c	\
	gtkemojicompletion.c	\
	gtkentry.c		\
	gtkentrybuffer.c	\
	gtkentrycompletion.c	\
	gtkeventbox.c		\
	gtkeventcontroller.c	\
	gtkeventcontrollerkey.c	\
	gtkeventcontrollermotion.c	\
	gtkeventcontrollerscroll.c	\
	gtkexpander.c		\
	gtkfilechooser.c	\
	gtkfilechooserbutton.c	\
	gtkfilechooserdialog.c	\
	gtkfilechooserembed.c	\
	gtkfilechooserentry.c	\
	gtkfilechoosernative.c	\
	gtkfilechoosernativeportal.c	\
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
	gtkgesturestylus.c	\
	gtkgestureswipe.c	\
	gtkgesturezoom.c	\
	gtkglarea.c		\
	gtkgrid.c		\
	gtkheaderbar.c		\
	gtkhsla.c		\
	gtkicon.c		\
	gtkiconcache.c		\
	gtkiconcachevalidator.c	\
	gtkiconhelper.c		\
	gtkicontheme.c		\
	gtkiconview.c		\
	gtkimage.c		\
	gtkimagedefinition.c	\
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
	gtknativedialog.c	\
	gtknotebook.c		\
	gtkoffscreenwindow.c	\
	gtkorientable.c		\
	gtkoverlay.c		\
	gtkpadcontroller.c	\
	gtkpagesetup.c		\
	gtkpaned.c		\
	gtkpango.c		\
	gtkpapersize.c		\
	gtkpathbar.c		\
	gtkplacessidebar.c	\
	gtkplacesview.c		\
	gtkplacesviewrow.c	\
	gtkprintcontext.c	\
	gtkprintoperation.c	\
	gtkprintoperationpreview.c \
	gtkprintsettings.c	\
	gtkprintutils.c		\
	gtkprivate.c		\
	gtkprogressbar.c	\
	gtkprogresstracker.c	\
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
	gtkshortcutsgroup.c	\
	gtkshortcutlabel.c	\
	gtkshortcutsshortcut.c	\
	gtkshortcutssection.c	\
	gtkshortcutswindow.c	\
	gtksidebarrow.c		\
	gtksizegroup.c		\
	gtksizerequest.c	\
	gtksizerequestcache.c	\
	gtkshow.c		\
	gtkstacksidebar.c	\
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
	gtktooltipwindow.c	\
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
	gtkutils.c		\
	gtkvolumebutton.c	\
	gtkviewport.c		\
	gtkwidget.c		\
	gtkwidgetpath.c		\
	gtkwindow.c		\
	gtkwindowgroup.c	\
	gtkwin32draw.c		\
	gtkwin32theme.c		\
	gdkpixbufutils.c	\
	language-names.c	\
	script-names.c

if USE_QUARTZ
gtk_base_c_sources += 		\
	gtkclipboard-quartz.c	\
	gtkdnd-quartz.c
else
gtk_base_c_sources += 		\
	gtkclipboard.c 		\
	gtkdnd.c		\
	gtkdragdest.c
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
	gtkprintoperation-portal.h	\
	gtkprintoperation-portal.c	\
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

if HAVE_TRACKER3
gtk_private_h_sources += \
	gtksearchenginetracker3.h
gtk_os_unix_c_sources += \
	gtksearchenginetracker3.c
endif

gtk_c_sources += $(gtk_os_unix_c_sources)
endif

gtk_os_win32_c_sources = \
	gtkprint-win32.c		\
	gtkprintoperation-win32.c	\
	gtkfilechoosernativewin32.c	\
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
	gtkfilechoosernativequartz.c \
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
	gtkmarshalers.c				\
	gtkprivatetypebuiltins.c

nodist_gtk_c_sources += $(gtk_built_sources)

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
	$(print_portal_built_sources)		\
	$(stamp_files)

DISTCLEANFILES = gtktypefuncs.inc

if OS_WIN32
DISTCLEANFILES += gtk-win32.rc
endif

EXTRA_DIST += $(gtk_all_private_h_sources) $(gtk_extra_sources)

# We need to dist this for `make distcheck`, apparently...
EXTRA_DIST += gtk.gresource.xml


pkgdatadir = $(datadir)/gtk-$(GTK_API_VERSION)
pkgdata_DATA = gtkbuilder.rng

EXTRA_DIST += gtkbuilder.rnc gtkbuilder.rng

itsdir = $(datadir)/gettext/its
its_DATA = gtkbuilder.loc gtkbuilder.its

EXTRA_DIST += $(its_DATA)

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
	ui/gtkcombobox.ui		\
	ui/gtkdialog.ui		\
	ui/gtkfilechooserbutton.ui	\
	ui/gtkemojichooser.ui		\
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
	ui/gtkplacesview.ui		\
	ui/gtkplacesviewrow.ui		\
	ui/gtkprintunixdialog.ui	\
	ui/gtkrecentchooserdefault.ui	\
	ui/gtksearchbar.ui		\
	ui/gtkscalebutton.ui		\
	ui/gtkstatusbar.ui		\
	ui/gtktooltipwindow.ui		\
	ui/gtkvolumebutton.ui		\
	ui/gtksidebarrow.ui

#
# rules to generate built sources
#
# setup autogeneration dependencies
gen_sources = xgen-gdef xgen-gtbh xgen-gtic xgen-gmh xgen-gmc xgen-gmlh xgen-gmlc xgen-gtfsrc.c xgen-gtf
CLEANFILES = $(gen_sources)

BUILT_SOURCES = \
	$(gtk_built_sources) \
	$(gtk_dbus_built_sources) \
	$(print_portal_built_sources)

# all autogenerated files need to be generated in the srcdir,
# so old versions get remade and are not confused with newer
# versions in the build dir. thus a development setup requires
# srcdir to be writable, passing --disable-rebuilds to
# ../configure will supress all autogeneration rules.
gtkmarshalers.h: stamp-gtkmarshalers.h
	@true
stamp-gtkmarshalers.h: gtkmarshalers.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) \
		--prefix=_gtk_marshal \
		--header \
		--valist-marshallers \
		$(srcdir)/gtkmarshalers.list >> xgen-gmlh \
	&& (cmp -s xgen-gmlh gtkmarshalers.h || cp xgen-gmlh gtkmarshalers.h) \
	&& rm -f xgen-gmlh \
	&& echo timestamp > $(@F)
gtkmarshalers.c: gtkmarshalers.list
	$(AM_V_GEN) (echo "#undef G_ENABLE_DEBUG"; \
	 $(GLIB_GENMARSHAL) \
	 	--prefix=_gtk_marshal \
		--body \
		--valist-marshallers $(srcdir)/gtkmarshalers.list) >> xgen-gmlc \
	&& cp xgen-gmlc gtkmarshalers.c \
	&& rm -f xgen-gmlc

gtktypebuiltins.h: stamp-gtktypebuiltins.h
	@true
stamp-gtktypebuiltins.h: $(gtk_public_h_sources) $(a11y_h_sources) $(gtk_deprecated_h_sources) gtktypebuiltins.h.template
	$(AM_V_GEN) ( cd $(srcdir) && $(GLIB_MKENUMS) --template gtktypebuiltins.h.template \
		$(gtk_public_h_sources) $(a11y_h_sources) $(gtk_deprecated_h_sources) ) > xgen-gtbh \
	&& (cmp -s xgen-gtbh gtktypebuiltins.h || cp xgen-gtbh gtktypebuiltins.h ) \
	&& rm -f xgen-gtbh \
	&& echo timestamp > $(@F)
gtktypebuiltins.c: $(gtk_public_h_sources) $(a11y_h_sources) $(gtk_deprecated_h_sources) gtktypebuiltins.c.template
	$(AM_V_GEN) ( cd $(srcdir) && $(GLIB_MKENUMS) --template gtktypebuiltins.c.template \
		$(gtk_public_h_sources) $(a11y_h_sources) $(gtk_deprecated_h_sources) ) > xgen-gtbc \
	&& cp xgen-gtbc gtktypebuiltins.c  \
	&& rm -f xgen-gtbc

gtk.gresource.xml: Makefile.am inspector/Makefile.inc
	$(AM_V_GEN) echo "<?xml version='1.0' encoding='UTF-8'?>" > $@; \
	echo "<gresources>" >> $@; \
	echo "  <gresource prefix='/org/gtk/libgtk'>" >> $@; \
        echo "    <file>theme/Adwaita/gtk.css</file>" >> $@; \
        echo "    <file>theme/Adwaita/gtk-dark.css</file>" >> $@; \
        echo "    <file>theme/Adwaita/gtk-contained.css</file>" >> $@; \
        echo "    <file>theme/Adwaita/gtk-contained-dark.css</file>" >> $@; \
        for f in $(srcdir)/theme/Adwaita/assets/*.png; do \
          n=`basename $$f`; \
          echo "    <file preprocess='to-pixdata'>theme/Adwaita/assets/$$n</file>" >> $@; \
        done; \
        for f in $(srcdir)/theme/Adwaita/assets/*.svg; do \
          n=`basename $$f`; \
          echo "    <file>theme/Adwaita/assets/$$n</file>" >> $@; \
        done; \
	echo "    <file>theme/HighContrast/gtk.css</file>" >> $@; \
	echo "    <file alias='theme/HighContrastInverse/gtk.css'>theme/HighContrast/gtk-inverse.css</file>" >> $@; \
	echo "    <file>theme/HighContrast/gtk-contained.css</file>" >> $@; \
	echo "    <file>theme/HighContrast/gtk-contained-inverse.css</file>" >> $@; \
	      for f in $(srcdir)/theme/HighContrast/assets/*.png; do \
          n=`basename $$f`; \
          echo "    <file preprocess='to-pixdata'>theme/HighContrast/assets/$$n</file>" >> $@; \
        done; \
        for f in $(srcdir)/theme/HighContrast/assets/*.svg; do \
          n=`basename $$f`; \
          echo "    <file>theme/HighContrast/assets/$$n</file>" >> $@; \
        done; \
	echo "    <file>theme/win32/gtk-win32-base.css</file>" >> $@; \
	echo "    <file>theme/win32/gtk.css</file>" >> $@; \
	for f in $(srcdir)/cursor/*.png; do \
	  n=`basename $$f`; \
	  echo "    <file>cursor/$$n</file>" >> $@; \
	done; \
	for f in $(srcdir)/gesture/*.symbolic.png; do \
	  n=`basename $$f`; \
	  echo "    <file alias='icons/64x64/actions/$$n'>gesture/$$n</file>" >> $@; \
	done; \
	for f in $(srcdir)/ui/*.ui; do \
	  n=`basename $$f`; \
	  echo "    <file preprocess='xml-stripblanks'>ui/$$n</file>" >> $@; \
	done; \
	for s in 16x16 22x22 24x24 32x32 48x48; do \
	  for c in actions status categories; do \
	    for f in $(srcdir)/icons/$$s/$$c/*.png; do \
	      test "$$f" = "$(srcdir)/icons/$$s/$$c/*.png" && continue; \
	      n=`basename $$f`; \
	      echo "    <file>icons/$$s/$$c/$$n</file>" >> $@; \
	    done; \
	  done; \
	done; \
	for s in scalable; do \
	  for c in status; do \
	    for f in $(srcdir)/icons/$$s/$$c/*.svg; do \
	      test "$$f" = "$(srcdir)/icons/$$s/$$c/*.svg" && continue; \
	      n=`basename $$f`; \
	      echo "    <file>icons/$$s/$$c/$$n</file>" >> $@; \
	    done; \
	  done; \
	done; \
	for f in $(srcdir)/inspector/*.ui; do \
	  n=`basename $$f`; \
	  echo "    <file compressed='true' preprocess='xml-stripblanks'>inspector/$$n</file>" >> $@; \
	done; \
	echo "    <file>inspector/logo.png</file>" >> $@; \
	for f in $(srcdir)/emoji/*.data; do \
	  n=`basename $$f`; \
	  echo "    <file>emoji/$$n</file>" >> $@; \
	done; \
	echo "  </gresource>" >> $@; \
	echo "</gresources>" >> $@;

gesture_sources = 				\
	gesture/gesture-pinch-symbolic.svg			\
	gesture/gesture-stretch-symbolic.svg			\
	gesture/gesture-rotate-clockwise-symbolic.svg		\
	gesture/gesture-rotate-anticlockwise-symbolic.svg	\
	gesture/gesture-two-finger-swipe-left-symbolic.svg	\
	gesture/gesture-two-finger-swipe-right-symbolic.svg

adwaita_theme_scss = \
	theme/Adwaita/_colors-public.scss 	\
	theme/Adwaita/_colors.scss 		\
	theme/Adwaita/_common.scss 		\
	theme/Adwaita/_drawing.scss 		\
	theme/Adwaita/gtk-contained-dark.scss 	\
	theme/Adwaita/gtk-contained.scss 	\
	$()

highcontrast_theme_scss = \
	theme/HighContrast/_colors-hc.scss 	\
	theme/HighContrast/gtk-contained-inverse.scss \
	theme/HighContrast/gtk-contained.scss 	\
	$()

if REBUILD_SCSS
# If we have sassc then we can rebuild the theme CSS as soon as the SCSS
# files have been changed

scss_verbose = $(scss_verbose_@AM_V@)
scss_verbose_ = $(scss_verbose_@AM_DEFAULT_V@)
scss_verbose_0 = @echo "  SCSS     $@";

SASSC_OPTS = -a -M -t compact

theme/Adwaita/gtk-contained.css: $(top_srcdir)/gtk/theme/Adwaita/gtk-contained.scss $(adwaita_theme_scss)
	$(scss_verbose) $(MKDIR_P) $(dir $@) && $(SASSC) $(SASSC_OPTS) $< $@ || rm -f $@

theme/Adwaita/gtk-contained-dark.css: $(top_srcdir)/gtk/theme/Adwaita/gtk-contained-dark.scss $(adwaita_theme_scss)
	$(scss_verbose) $(MKDIR_P) $(dir $@) && $(SASSC) $(SASSC_OPTS) $< $@ || rm -f $@

theme/HighContrast/gtk-contained.css: $(top_srcdir)/gtk/theme/HighContrast/gtk-contained.scss $(highcontrast_theme_scss)
	$(scss_verbose) $(MKDIR_P) $(dir $@) && $(SASSC) $(SASSC_OPTS) $< $@ || rm -f $@

theme/HighContrast/gtk-contained-inverse.css: $(top_srcdir)/gtk/theme/HighContrast/gtk-contained-inverse.scss $(highcontrast_theme_scss)
	$(scss_verbose) $(MKDIR_P) $(dir $@) && $(SASSC) $(SASSC_OPTS) $< $@ || rm -f $@

endif # REBUILD_SCSS

theme_sources = 				\
	theme/win32/gtk.css	 		\
	theme/win32/gtk-win32-base.css 		\
	$(adwaita_theme_scss)			\
	theme/Adwaita/assets.txt 		\
	theme/Adwaita/assets.svg 		\
	theme/Adwaita/README 			\
	theme/Adwaita/gtk-contained.css 	\
	theme/Adwaita/gtk-contained-dark.css 	\
	theme/Adwaita/gtk.css 			\
	theme/Adwaita/gtk-dark.css 		\
	$(highcontrast_theme_scss)		\
	theme/HighContrast/gtk-contained.css 	\
	theme/HighContrast/gtk-contained-inverse.css \
	theme/HighContrast/gtk.css 		\
	theme/HighContrast/gtk-inverse.css

resource_files = $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(builddir)/gtk.gresource.xml)

gtkresources.h: gtk.gresource.xml
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $< \
		--target=$@ --sourcedir=$(srcdir) --c-name _gtk --generate-header --manual-register --internal
gtkresources.c: gtk.gresource.xml $(resource_files)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) $< \
		--target=$@ --sourcedir=$(srcdir) --c-name _gtk --generate-source --manual-register --internal

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


gtktypefuncs.inc: stamp-gtktypebuiltins.h stamp-gtkprivatetypebuiltins.h $(top_srcdir)/gtk/*.h $(top_srcdir)/gtk/a11y/*.h $(top_srcdir)/gtk/deprecated/*.h $(top_srcdir)/gdk/*.h Makefile
	$(AM_V_GEN) export LC_ALL=C ; \
	  (echo '#undef GTK_COMPILATION' && echo '#include <gtk/gtkx.h>') > xgen-gtfsrc.c && \
	  echo 'G_GNUC_BEGIN_IGNORE_DEPRECATIONS' > xgen-gtf && \
	  ${CPP} $(DEFS) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) xgen-gtfsrc.c | \
	  $(GREP) -o '\bg[td]k_[a-zA-Z0-9_]*_get_type\b' | \
	  sort | uniq | \
	  $(SED) '{ s/^/*tp++ = /; s/$$/();/; s/^.*\(gdk_x11\|gtk_plug_\|gtk_socket_\).*$$/#ifdef GDK_WINDOWING_X11\n&\n#endif/; }' >> xgen-gtf \
	&& cp xgen-gtf $@ && rm -f xgen-gtf
$(srcdir)/gtktestutils.c: gtktypefuncs.inc

# target platform:
lib_LTLIBRARIES = libgtk-3.la

gtkincludedir = $(includedir)/gtk-3.0/gtk
gtkinclude_HEADERS = $(gtk_public_h_sources) $(gtk_semi_private_h_sources) $(gtk_built_public_sources) gtkversion.h

a11yincludedir = $(includedir)/gtk-3.0/gtk/a11y
a11yinclude_HEADERS= $(a11y_h_sources)

deprecatedincludedir = $(includedir)/gtk-3.0/gtk/deprecated
deprecatedinclude_HEADERS= $(gtk_deprecated_h_sources)

gtkunixprintincludedir = $(includedir)/gtk-3.0/unix-print/gtk
gtkunixprintinclude_HEADERS = $(gtk_unix_print_public_h_sources)

nodist_libgtk_3_la_SOURCES = $(nodist_gtk_c_sources)
libgtk_3_la_SOURCES = $(gtk_c_sources)
libgtk_3_la_CFLAGS = $(AM_CFLAGS) $(GDK_HIDDEN_VISIBILITY_CFLAGS)
libgtk_3_la_LDFLAGS = $(libtool_opts)
libgtk_3_la_LIBADD = $(libadd)
libgtk_3_la_DEPENDENCIES = $(deps)

if USE_QUARTZ
libgtk_3_la_CFLAGS += "-xobjective-c"
endif

if USE_WIN32
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

# ------------------- MSVC Build Items ----------------
MSVCPROJS = gtk-3

gtk_3_FILES = \
	$(gtk_base_c_sources)		\
	$(gtk_os_win32_c_sources)	\
	$(gtk_use_win32_c_sources)

gtk_3_EXCLUDES = dummy
gtk_3_HEADERS_DIR = $(gtkincludedir)

gtk_3_HEADERS_INST = \
	$(filter-out gtkversion.h, $(gtkinclude_HEADERS))	\
	$(a11y_h_sources)	\
	$(gtk_deprecated_h_sources)

gtk_3_HEADERS_EXCLUDES = gtktypebuiltins.h

include $(top_srcdir)/win32/Makefile.msvcproj

if HAVE_INTROSPECTION
# Introspection Items for MSVC
MSVC_INTROSPECT_GIRS = Gtk-3.0.gir

GTK_MSVC_GIR_CFLAGS =						\
	$(GTK_BASE_CFLAGS_DEFINES)				\
	-DGTK_LIBDIR=\\\"/dummy/lib\\\"				\
	-DGTK_DATADIR=\\\"/dummy/share\\\"			\
	-DGTK_DATA_PREFIX=\\\"/dummy\\\"			\
	-DGTK_SYSCONFDIR=\\\"/dummy/etc\\\"			\
	-DGTK_HOST=\\\"'$$$$(AT_PLAT)'-pc-vs'$$$$(VSVER)'\\\"	\
	-DGTK_PRINT_BACKENDS=\\\"file\\\"			\
	-DINCLUDE_IM_am_et			\
	-DINCLUDE_IM_cedilla			\
	-DINCLUDE_IM_cyrillic_translit		\
	-DINCLUDE_IM_ime			\
	-DINCLUDE_IM_inuktitu			\
	-DINCLUDE_IM_ipa			\
	-DINCLUDE_IM_multipress			\
	-DINCLUDE_IM_thai			\
	-DINCLUDE_IM_ti_er			\
	-DINCLUDE_IM_ti_et			\
	-DINCLUDE_IM_viqr			\
	-DGTK_TEXT_USE_INTERNAL_UNSUPPORTED_API	\
	-I'./vs$$$$(VSVER)/$$$$(CFG)/$$$$(PLAT)/obj/gtk-3'	\
	-I'./vs$$$$(VSVER)/$$$$(CFG)/$$$$(PLAT)/obj/gdk-3' -I..	\
	-I'./vs$$$$(VSVER)/$$$$(CFG)/$$$$(PLAT)/obj/gtk-3/gtk' -I../gtk	\
	-I'./vs$$$$(VSVER)/$$$$(CFG)/$$$$(PLAT)/obj/gdk-3/gdk' -I../gdk

INTROSPECTION_INTERMEDIATE_ITEMS = \
	$(top_builddir)/win32/Gtk-3.0.gir.msvc.introspect		\
	$(top_builddir)/win32/Gtk_3_0_gir_list

Gtk_3_0_gir_MSVC_FILES =		\
	$(filter-out gtktypebuiltins.h gtkversion.h, $(introspected_pub_headers)) \
	$(gtk_base_c_sources)		\
	$(gtk_os_win32_c_sources)

Gtk_3_0_gir_MSVC_EXPORT_PACKAGES = $(Gtk_3_0_gir_EXPORT_PACKAGES)
Gtk_3_0_gir_MSVC_INCLUDE_GIRS = Atk-1.0
Gtk_3_0_gir_MSVC_LIBS = gtk-3.0 gdk-3.0
Gtk_3_0_gir_MSVC_CFLAGS = $(GTK_MSVC_GIR_CFLAGS)
Gtk_3_0_gir_MSVC_SCANNERFLAGS = --warn-all --include-uninstalled='./vs$$$$(VSVER)/$$$$(CFG)/$$$$(PLAT)/bin/Gdk-3.0.gir'

include $(top_srcdir)/win32/Makefile.msvc-introspection

else
INTROSPECTION_INTERMEDIATE_ITEMS =

endif

dist-hook: \
	$(top_builddir)/win32/vs9/gtk-3.vcproj	\
	$(top_builddir)/win32/vs9/gtk-3.headers	\
	$(INTROSPECTION_INTERMEDIATE_ITEMS)

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
introspected_pub_headers = $(filter-out %private.h gtktextdisplay.h gtktextlayout.h gtkx.h, $(gtkinclude_HEADERS) $(a11yinclude_HEADERS) $(deprecatedinclude_HEADERS))

introspection_files = \
    $(introspected_pub_headers) \
    $(filter-out %win32.c, $(gtk_base_c_sources)) \
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
Gtk_3_0_gir_C_INCLUDES = gtk/gtk.h gtk/gtk-a11y.h
if USE_X11
Gtk_3_0_gir_SCANNERFLAGS += --add-include-path=$(top_builddir)/gdk/x11
Gtk_3_0_gir_INCLUDES += xlib-2.0
Gtk_3_0_gir_C_INCLUDES += gtk/gtkx.h
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
	gtk-builder-tool \
	gtk-query-settings \
	gtk-launch

gtk_query_immodules_3_0_SOURCES = queryimmodules.c gtkutils.c
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

gtk_builder_tool_SOURCES = gtk-builder-tool.c
gtk_builder_tool_LDADD =			\
	libgtk-3.la				\
	$(top_builddir)/gdk/libgdk-3.la		\
	$(GTK_DEP_LIBS)

gtk_query_settings_SOURCES = gtk-query-settings.c
gtk_query_settings_LDADD= 			\
	libgtk-3.la				\
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

$(GTK_UPDATE_ICON_CACHE_MANIFEST): Makefile generate-uac-manifest.py
	$(PYTHON) $(srcdir)/generate-uac-manifest.py -p=gtk3 -n=gtk-update-icon-cache --pkg-version=$(GTK_VERSION) --output-dir=$(builddir)

$(GTK_UPDATE_ICON_CACHE_RC): $(GTK_UPDATE_ICON_CACHE_MANIFEST)

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
	org.gtk.Settings.EmojiChooser.gschema.xml \
	org.gtk.Settings.Debug.gschema.xml

@GSETTINGS_RULES@

EXTRA_DIST +=                   \
	$(resource_files)	\
	$(gesture_sources)	\
	$(theme_sources)	\
	$(gsettings_SCHEMAS)	\
	compose/compose-parse.py	\
	emoji/convert-emoji.c	\
	emoji/gresource.xml.in	\
	emoji/README.md	\
	emoji/de.data	\
	emoji/en.data	\
	emoji/es.data	\
	emoji/fr.data	\
	emoji/zh.data	\
	a11y/Makefile.inc	\
	deprecated/Makefile.inc	\
	glade/README.glade	\
	glade/gtk-private-widgets.xml \
	compose/gtk-compose-lookaside.txt \
	inspector/Makefile.inc	\
	libgtk3.manifest.in	\
	gtk-win32.rc.body.in	\
	gtk-win32.rc.body	\
	gtkwin32embed.h		\
	gtkwin32embedwidget.h	\
	gtkwin32embedwidget.c	\
	gtkprint-win32.h	\
	gtkprint-win32.c	\
	gtksearchenginetracker3.c \
	gtksearchenginetracker3.h \
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
	gtkprivatetypebuiltins.h.template \
	meson.build \
	gen-gtk-gresources-xml.py \
	gen-rc.py \
	generate-uac-manifest.py \
	gentypefuncs.py \
	a11y/meson.build \
	deprecated/meson.build \
	inspector/meson.build \
	theme/Adwaita/meson.build \
	theme/Adwaita/parse-sass.sh \
	theme/Adwaita/render-assets.sh \
	theme/HighContrast/assets.svg \
	theme/HighContrast/assets.txt \
	theme/HighContrast/meson.build \
	theme/HighContrast/parse-sass.sh \
	theme/HighContrast/render-assets.sh


-include $(top_srcdir)/git.mk
