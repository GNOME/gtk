# List of headers to be passed into glib-mkenums


# GTK+ header files for public installation (non-generated, or generated
# by configure)

GTK_PUB_HDRS = 		\
	gtk.h			\
	gtk-autocleanups.h	\
	gtkx.h			\
	gtkx-autocleanups.h	\
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
	gtkdragdest.h		\
	gtkdragsource.h		\
	gtkdrawingarea.h	\
	gtkeditable.h		\
	gtkentry.h		\
	gtkentrybuffer.h	\
	gtkentrycompletion.h	\
	gtkenums.h		\
	gtkeventbox.h		\
	gtkeventcontroller.h	\
	gtkeventcontrollerkey.h	\
	gtkeventcontrollermotion.h	\
	gtkeventcontrollerscroll.h	\
	gtkexpander.h		\
	gtkfilechooser.h	\
	gtkfilechooserbutton.h	\
	gtkfilechooserdialog.h	\
	gtkfilechoosernative.h	\
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
	gtkgesturestylus.h	\
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
	gtknativedialog.h	\
	gtknotebook.h		\
	gtkoffscreenwindow.h	\
	gtkorientable.h		\
	gtkoverlay.h		\
	gtkpadcontroller.h	\
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
	gtkshortcutlabel.h	\
	gtkshortcutsgroup.h	\
	gtkshortcutssection.h	\
	gtkshortcutsshortcut.h	\
	gtkshortcutswindow.h	\
	gtkshow.h		\
	gtkstacksidebar.h	\
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

GTK_PRIVATE_TYPE_HDRS =	\
	gtkcsstypesprivate.h	\
	gtktexthandleprivate.h

GTK_UNIX_PRINT_PUB_HDRS = 	\
	gtkpagesetupunixdialog.h	\
	gtkprintunixdialog.h		\
	gtkprinter.h			\
	gtkprintjob.h			\
	gtkunixprint-autocleanups.h	\
	gtkunixprint.h

# Installed header files without compatibility guarantees
# that are not included in gtk/gtk.h
gtk_semi_private_h_sources = \
	gtktextlayout.h

# Base non-generated private headers
gtk_base_private_h_sources =	\
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

# GTK base sources (had to split those in 2 parts to avoid U1095 error :|, order unchanged from before)
# gtk_base_c_sources_base_gtka_gtkh: sources from gtka* to gtkh*, mostly
# gtk_base_c_sources_base_gtki_gtkw: sources from gtki* to gtkw*, mostly
gtk_base_c_sources_base_gtka_gtkh =	\
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
	gtkhsla.c

gtk_base_c_sources_base_gtki_gtkw =	\
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
	gtkinvisible.c	\
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

gtk_base_c_sources_base = $(gtk_base_c_sources_base_gtka_gtkh) $(gtk_base_c_sources_base_gtki_gtkw)

# Clipboard/dnd platform-specific sources
gtk_clipboard_dnd_c_sources_quartz =	\
	gtkclipboard-quartz.c	\
	gtkdnd-quartz.c

gtk_clipboard_dnd_c_sources_generic =	\
	gtkclipboard.c 		\
	gtkdnd.c		\
	gtkdragdest.c

# Stub sources
gtk_use_stub_c_sources = 	\
	gtkmountoperation-stub.c
	
# Base platform-specific sources
gtk_os_unix_c_sources_base =	\
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

gtk_os_unix_private_h_sources =	\
	gtkiconcachevalidator.h		\
	gtkprintbackend.h		\
	gtkprinter-private.h		\
	gtkprinteroption.h		\
	gtkprinteroptionset.h		\
	gtkprinteroptionwidget.h	\
	gtksearchenginetracker.h

gtk_os_unix_tracker3_private_h_sources = gtksearchenginetracker3.h
gtk_os_unix_tracker3_c_sources = gtksearchenginetracker3.c

gtk_use_wayland_c_sources = \
	gtkapplication-wayland.c

gtk_use_wayland_or_x11_c_sources = \
	gtkapplication-dbus.c

gtk_use_x11_c_sources = 	\
	gtkplug.c		\
	gtksocket.c		\
	gtkxembed.c		\
	deprecated/gtktrayicon-x11.c	\
	gtkapplication-x11.c	\
	gtkmountoperation-x11.c

gtk_use_x11_private_h_sources = 	\
	gtkxembed.h		\
	deprecated/gtktrayicon.h	\
	xembed.h

gtk_os_win32_c_sources = \
	gtkprint-win32.c		\
	gtkprintoperation-win32.c	\
	gtkfilechoosernativewin32.c	\
	gtkwin32.c

gtk_os_win32_private_h_sources = gtkprint-win32.h

gtk_use_win32_private_h_sources = 	\
	gtkwin32embed.h		\
	gtkwin32embedwidget.h

gtk_use_win32_c_sources = 	\
	gtkwin32embed.c		\
	gtkwin32embedwidget.c	\
	$(gtk_use_stub_c_sources)

gtk_use_quartz_c_sources = 	\
	gtksearchenginequartz.c	\
	$(gtk_use_stub_c_sources) \
	gtkapplication-quartz.c \
	gtkapplication-quartz-menu.c \
	gtkfilechoosernativequartz.c \
	gtkquartz.c

gtk_use_quartz_private_h_sources = 	\
	gtksearchenginequartz.h	\
	gtkquartz.h

# Generated sources
# built sources that get installed with the header files
gtk_built_public_sources =			\
	gtktypebuiltins.h

# built headers that don't get installed
gtk_dbus_built_sources = gtkdbusgenerated.c gtkdbusgenerated.h

gtk_built_private_headers =			\
	gtkresources.h				\
	gtkmarshalers.h				\
	gtkprivatetypebuiltins.h

gtk_other_built_sources =	\
	gtkresources.c				\
	gtk.gresource.xml			\
	gtktypebuiltins.c			\
	gtkmarshalers.c				\
	gtkprivatetypebuiltins.c

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

# Resources
# Base UI templates
gtk_base_ui_templates =	\
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

gesture_sources = 				\
	gesture/gesture-pinch-symbolic.svg			\
	gesture/gesture-stretch-symbolic.svg			\
	gesture/gesture-rotate-clockwise-symbolic.svg		\
	gesture/gesture-rotate-anticlockwise-symbolic.svg	\
	gesture/gesture-two-finger-swipe-left-symbolic.svg	\
	gesture/gesture-two-finger-swipe-right-symbolic.svg

adwaita_theme_css_sources =	\
	theme/Adwaita/gtk-contained.css 	\
	theme/Adwaita/gtk-contained-dark.css 	\
	theme/Adwaita/gtk.css 			\
	theme/Adwaita/gtk-dark.css

adwaita_theme_scss_sources =	\
	theme/Adwaita/_colors-public.scss 	\
	theme/Adwaita/_colors.scss 		\
	theme/Adwaita/_common.scss 		\
	theme/Adwaita/_drawing.scss 		\
	theme/Adwaita/gtk-contained-dark.scss 	\
	theme/Adwaita/gtk-contained.scss

adwaita_theme_files =	\
	theme/Adwaita/assets.txt 		\
	theme/Adwaita/assets.svg 		\
	theme/Adwaita/README 			\
	$(adwaita_theme_css_sources)

highcontrast_theme_css_sources =	\
	theme/HighContrast/gtk-contained.css 	\
	theme/HighContrast/gtk-contained-inverse.css \
	theme/HighContrast/gtk.css 		\
	theme/HighContrast/gtk-inverse.css

highcontrast_theme_scss_sources =	\
	theme/HighContrast/_colors-hc.scss 	\
	theme/HighContrast/gtk-contained-inverse.scss \
	theme/HighContrast/gtk-contained.scss

win32_theme_css_sources =	\
	theme/win32/gtk.css	 		\
	theme/win32/gtk-win32-base.css
	
