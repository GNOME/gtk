subdir('deprecated')
subdir('a11y')
subdir('inspector')

gtk_cargs = [
  '-DGTK_COMPILATION',
  '-DG_LOG_DOMAIN="Gtk"',
  '-DGTK_PRINT_BACKEND_ENABLE_UNSUPPORTED',
  '-DGTK_BINARY_VERSION="@0@"'.format(gtk_binary_version),
  '-DGTK_HOST="@0@"'.format(host_machine.system()),
  '-DGTK_DATA_PREFIX="@0@"'.format(gtk_prefix),
  '-DGTK_LIBDIR="@0@"'.format(gtk_libdir),
  '-DGTK_LOCALEDIR="@0@"'.format(gtk_localedir),
  '-DGTK_DATADIR="@0@"'.format(gtk_datadir),
  '-DGTK_SYSCONFDIR="@0@"'.format(gtk_sysconfdir),
]

# List of sources to build the library from
gtk_sources = files(
  'gtkactionmuxer.c',
  'gtkactionobserver.c',
  'gtkactionobservable.c',
  'gtkactionable.c',
  'gtkquery.c',
  'gtksearchbar.c',
  'gtksearchentry.c',
  'gtksearchengine.c',
  'gtksearchenginesimple.c',
  'gtksearchenginemodel.c',
  'fnmatch.c',
  'gtkaboutdialog.c',
  'gtkaccelgroup.c',
  'gtkaccellabel.c',
  'gtkaccelmap.c',
  'gtkaccessible.c',
  'gtkactionbar.c',
  'gtkactionhelper.c',
  'gtkadjustment.c',
  'gtkallocatedbitmask.c',
  'gtkappchooser.c',
  'gtkappchooserwidget.c',
  'gtkappchooserbutton.c',
  'gtkappchooserdialog.c',
  'gtkapplication.c',
  'gtkapplicationaccels.c',
  'gtkapplicationimpl.c',
  'gtkapplicationwindow.c',
  'gtkaspectframe.c',
  'gtkassistant.c',
  'gtkbbox.c',
  'gtkbin.c',
  'gtkbindings.c',
  'gtkbookmarksmanager.c',
  'gtkborder.c',
  'gtkbox.c',
  'gtkboxgadget.c',
  'gtkbuildable.c',
  'gtkbuilder.c',
  'gtkbuilderparser.c',
  'gtkbuilder-menus.c',
  'gtkbuiltinicon.c',
  'gtkbutton.c',
  'gtkcairoblur.c',
  'gtkcalendar.c',
  'gtkcellarea.c',
  'gtkcellareabox.c',
  'gtkcellareaboxcontext.c',
  'gtkcellareacontext.c',
  'gtkcelleditable.c',
  'gtkcelllayout.c',
  'gtkcellrenderer.c',
  'gtkcellrendereraccel.c',
  'gtkcellrenderercombo.c',
  'gtkcellrendererpixbuf.c',
  'gtkcellrendererprogress.c',
  'gtkcellrendererspin.c',
  'gtkcellrendererspinner.c',
  'gtkcellrenderertext.c',
  'gtkcellrenderertoggle.c',
  'gtkcellview.c',
  'gtkcheckbutton.c',
  'gtkcheckmenuitem.c',
  'gtkcolorbutton.c',
  'gtkcolorchooser.c',
  'gtkcolorchooserwidget.c',
  'gtkcolorchooserdialog.c',
  'gtkcoloreditor.c',
  'gtkcolorpicker.c',
  'gtkcolorpickerkwin.c',
  'gtkcolorpickerportal.c',
  'gtkcolorpickershell.c',
  'gtkcolorplane.c',
  'gtkcolorscale.c',
  'gtkcolorswatch.c',
  'gtkcolorutils.c',
  'gtkcombobox.c',
  'gtkcomboboxtext.c',
  'gtkcomposetable.c',
  'gtkcontainer.c',
  'gtkcssanimation.c',
  'gtkcssanimatedstyle.c',
  'gtkcssarrayvalue.c',
  'gtkcssbgsizevalue.c',
  'gtkcssbordervalue.c',
  'gtkcsscalcvalue.c',
  'gtkcsscolorvalue.c',
  'gtkcsscornervalue.c',
  'gtkcsscustomgadget.c',
  'gtkcsscustomproperty.c',
  'gtkcssdimensionvalue.c',
  'gtkcsseasevalue.c',
  'gtkcssenumvalue.c',
  'gtkcssenginevalue.c',
  'gtkcssgadget.c',
  'gtkcssiconthemevalue.c',
  'gtkcssimage.c',
  'gtkcssimagebuiltin.c',
  'gtkcssimagecrossfade.c',
  'gtkcssimagefallback.c',
  'gtkcssimagegradient.c',
  'gtkcssimageicontheme.c',
  'gtkcssimagelinear.c',
  'gtkcssimageradial.c',
  'gtkcssimagesurface.c',
  'gtkcssimageurl.c',
  'gtkcssimagerecolor.c',
  'gtkcssimagescaled.c',
  'gtkcssimagevalue.c',
  'gtkcssimagewin32.c',
  'gtkcssinheritvalue.c',
  'gtkcssinitialvalue.c',
  'gtkcsskeyframes.c',
  'gtkcsslookup.c',
  'gtkcssmatcher.c',
  'gtkcssnode.c',
  'gtkcssnodedeclaration.c',
  'gtkcssnodestylecache.c',
  'gtkcssnumbervalue.c',
  'gtkcsspalettevalue.c',
  'gtkcssparser.c',
  'gtkcsspathnode.c',
  'gtkcsspositionvalue.c',
  'gtkcssprovider.c',
  'gtkcssrepeatvalue.c',
  'gtkcssrgbavalue.c',
  'gtkcsssection.c',
  'gtkcssselector.c',
  'gtkcssstringvalue.c',
  'gtkcssstyle.c',
  'gtkcssstylechange.c',
  'gtkcssshadowsvalue.c',
  'gtkcssshadowvalue.c',
  'gtkcssshorthandproperty.c',
  'gtkcssshorthandpropertyimpl.c',
  'gtkcssstaticstyle.c',
  'gtkcssstylefuncs.c',
  'gtkcssstyleproperty.c',
  'gtkcssstylepropertyimpl.c',
  'gtkcsstransformvalue.c',
  'gtkcsstransientnode.c',
  'gtkcsstransition.c',
  'gtkcsstypedvalue.c',
  'gtkcssunsetvalue.c',
  'gtkcsstypes.c',
  'gtkcssvalue.c',
  'gtkcsswidgetnode.c',
  'gtkcsswin32sizevalue.c',
  'gtkdialog.c',
  'gtkdragsource.c',
  'gtkdrawingarea.c',
  'gtkeditable.c',
  'gtkemojichooser.c',
  'gtkemojicompletion.c',
  'gtkentry.c',
  'gtkentrybuffer.c',
  'gtkentrycompletion.c',
  'gtkeventbox.c',
  'gtkeventcontroller.c',
  'gtkeventcontrollerkey.c',
  'gtkeventcontrollermotion.c',
  'gtkeventcontrollerscroll.c',
  'gtkexpander.c',
  'gtkfilechooser.c',
  'gtkfilechooserbutton.c',
  'gtkfilechooserdialog.c',
  'gtkfilechooserembed.c',
  'gtkfilechooserentry.c',
  'gtkfilechoosernative.c',
  'gtkfilechoosernativeportal.c',
  'gtkfilechooserutils.c',
  'gtkfilechooserwidget.c',
  'gtkfilefilter.c',
  'gtkfilesystem.c',
  'gtkfilesystemmodel.c',
  'gtkfixed.c',
  'gtkflowbox.c',
  'gtkfontbutton.c',
  'gtkfontchooser.c',
  'gtkfontchooserdialog.c',
  'gtkfontchooserutils.c',
  'gtkfontchooserwidget.c',
  'gtkframe.c',
  'gtkgladecatalog.c',
  'gtkgesture.c',
  'gtkgesturedrag.c',
  'gtkgesturelongpress.c',
  'gtkgesturemultipress.c',
  'gtkgesturepan.c',
  'gtkgesturerotate.c',
  'gtkgesturesingle.c',
  'gtkgesturestylus.c',
  'gtkgestureswipe.c',
  'gtkgesturezoom.c',
  'gtkglarea.c',
  'gtkgrid.c',
  'gtkheaderbar.c',
  'gtkhsla.c',
  'gtkicon.c',
  'gtkiconcache.c',
  'gtkiconcachevalidator.c',
  'gtkiconhelper.c',
  'gtkicontheme.c',
  'gtkiconview.c',
  'gtkimage.c',
  'gtkimagedefinition.c',
  'gtkimcontext.c',
  'gtkimcontextsimple.c',
  'gtkimmodule.c',
  'gtkimmulticontext.c',
  'gtkinfobar.c',
  'gtkinvisible.c',
  'gtkkeyhash.c',
  'gtkkineticscrolling.c',
  'gtklabel.c',
  'gtklayout.c',
  'gtklevelbar.c',
  'gtklinkbutton.c',
  'gtklistbox.c',
  'gtkliststore.c',
  'gtklockbutton.c',
  'gtkmain.c',
  'gtkmagnifier.c',
  'gtkmenu.c',
  'gtkmenubar.c',
  'gtkmenubutton.c',
  'gtkmenuitem.c',
  'gtkmenusectionbox.c',
  'gtkmenushell.c',
  'gtkmenutracker.c',
  'gtkmenutrackeritem.c',
  'gtkmenutoolbutton.c',
  'gtkmessagedialog.c',
  'gtkmnemonichash.c',
  'gtkmodelmenuitem.c',
  'gtkmodelbutton.c',
  'gtkmodifierstyle.c',
  'gtkmodules.c',
  'gtkmountoperation.c',
  'gtknativedialog.c',
  'gtknotebook.c',
  'gtkoffscreenwindow.c',
  'gtkorientable.c',
  'gtkoverlay.c',
  'gtkpadcontroller.c',
  'gtkpagesetup.c',
  'gtkpaned.c',
  'gtkpango.c',
  'gtkpapersize.c',
  'gtkpathbar.c',
  'gtkplacessidebar.c',
  'gtkplacesview.c',
  'gtkplacesviewrow.c',
  'gtkprintcontext.c',
  'gtkprintoperation.c',
  'gtkprintoperationpreview.c',
  'gtkprintsettings.c',
  'gtkprintutils.c',
  'gtkprivate.c',
  'gtkprogressbar.c',
  'gtkprogresstracker.c',
  'gtkpixelcache.c',
  'gtkpopover.c',
  'gtkpopovermenu.c',
  'gtkradiobutton.c',
  'gtkradiomenuitem.c',
  'gtkradiotoolbutton.c',
  'gtkrange.c',
  'gtkrbtree.c',
  'gtkrecentchooserdefault.c',
  'gtkrecentchooserdialog.c',
  'gtkrecentchoosermenu.c',
  'gtkrecentchooserwidget.c',
  'gtkrecentchooserutils.c',
  'gtkrecentchooser.c',
  'gtkrecentfilter.c',
  'gtkrecentmanager.c',
  'gtkrender.c',
  'gtkrenderbackground.c',
  'gtkrenderborder.c',
  'gtkrendericon.c',
  'gtkrevealer.c',
  'gtkroundedbox.c',
  'gtkscale.c',
  'gtkscalebutton.c',
  'gtkscrollable.c',
  'gtkscrollbar.c',
  'gtkscrolledwindow.c',
  'gtkselection.c',
  'gtkseparator.c',
  'gtkseparatormenuitem.c',
  'gtkseparatortoolitem.c',
  'gtksettings.c',
  'gtkshortcutsgroup.c',
  'gtkshortcutlabel.c',
  'gtkshortcutsshortcut.c',
  'gtkshortcutssection.c',
  'gtkshortcutswindow.c',
  'gtksidebarrow.c',
  'gtksizegroup.c',
  'gtksizerequest.c',
  'gtksizerequestcache.c',
  'gtkshow.c',
  'gtkstacksidebar.c',
  'gtkspinbutton.c',
  'gtkspinner.c',
  'gtkstack.c',
  'gtkstackswitcher.c',
  'gtkstatusbar.c',
  'gtkstyleanimation.c',
  'gtkstylecascade.c',
  'gtkstylecontext.c',
  'gtkstyleproperty.c',
  'gtkstyleprovider.c',
  'gtkstyleproviderprivate.c',
  'gtkswitch.c',
  'gtktestutils.c',
  'gtktextattributes.c',
  'gtktextbtree.c',
  'gtktextbuffer.c',
  'gtktextbufferrichtext.c',
  'gtktextbufferserialize.c',
  'gtktextchild.c',
  'gtktextdisplay.c',
  'gtktexthandle.c',
  'gtktextiter.c',
  'gtktextlayout.c',
  'gtktextmark.c',
  'gtktextsegment.c',
  'gtktexttag.c',
  'gtktexttagtable.c',
  'gtktexttypes.c',
  'gtktextutil.c',
  'gtktextview.c',
  'gtktogglebutton.c',
  'gtktoggletoolbutton.c',
  'gtktoolbar.c',
  'gtktoolbutton.c',
  'gtktoolitem.c',
  'gtktoolitemgroup.c',
  'gtktoolpalette.c',
  'gtktoolshell.c',
  'gtktooltip.c',
  'gtktooltipwindow.c',
  'gtktrashmonitor.c',
  'gtktreedatalist.c',
  'gtktreednd.c',
  'gtktreemenu.c',
  'gtktreemodel.c',
  'gtktreemodelfilter.c',
  'gtktreemodelsort.c',
  'gtktreeselection.c',
  'gtktreesortable.c',
  'gtktreestore.c',
  'gtktreeview.c',
  'gtktreeviewcolumn.c',
  'gtkutils.c',
  'gtkvolumebutton.c',
  'gtkviewport.c',
  'gtkwidget.c',
  'gtkwidgetpath.c',
  'gtkwindow.c',
  'gtkwindowgroup.c',
  'gtkwin32draw.c',
  'gtkwin32theme.c',
  'gdkpixbufutils.c',
  'language-names.c',
  'script-names.c',
)

gtk_private_type_headers = files(
  'gtkcsstypesprivate.h',
  'gtktexthandleprivate.h',
)

gtk_gir_public_headers = files(
  'gtk.h',
  'gtk-autocleanups.h',
  'gtkx-autocleanups.h',
  'gtk-a11y.h',
  'gtkaboutdialog.h',
  'gtkaccelgroup.h',
  'gtkaccellabel.h',
  'gtkaccelmap.h',
  'gtkaccessible.h',
  'gtkactionable.h',
  'gtkactionbar.h',
  'gtkadjustment.h',
  'gtkappchooser.h',
  'gtkappchooserbutton.h',
  'gtkappchooserdialog.h',
  'gtkappchooserwidget.h',
  'gtkapplication.h',
  'gtkapplicationwindow.h',
  'gtkaspectframe.h',
  'gtkassistant.h',
  'gtkbbox.h',
  'gtkbin.h',
  'gtkbindings.h',
  'gtkborder.h',
  'gtkbox.h',
  'gtkbuilder.h',
  'gtkbuildable.h',
  'gtkbutton.h',
  'gtkcalendar.h',
  'gtkcellarea.h',
  'gtkcellareacontext.h',
  'gtkcellareabox.h',
  'gtkcelleditable.h',
  'gtkcelllayout.h',
  'gtkcellrenderer.h',
  'gtkcellrendereraccel.h',
  'gtkcellrenderercombo.h',
  'gtkcellrendererpixbuf.h',
  'gtkcellrendererprogress.h',
  'gtkcellrendererspin.h',
  'gtkcellrendererspinner.h',
  'gtkcellrenderertext.h',
  'gtkcellrenderertoggle.h',
  'gtkcellview.h',
  'gtkcheckbutton.h',
  'gtkcheckmenuitem.h',
  'gtkclipboard.h',
  'gtkcolorbutton.h',
  'gtkcolorchooser.h',
  'gtkcolorchooserwidget.h',
  'gtkcolorchooserdialog.h',
  'gtkcolorutils.h',
  'gtkcombobox.h',
  'gtkcomboboxtext.h',
  'gtkcontainer.h',
  'gtkcssprovider.h',
  'gtkcsssection.h',
  'gtkdebug.h',
  'gtkdialog.h',
  'gtkdnd.h',
  'gtkdragdest.h',
  'gtkdragsource.h',
  'gtkdrawingarea.h',
  'gtkeditable.h',
  'gtkentry.h',
  'gtkentrybuffer.h',
  'gtkentrycompletion.h',
  'gtkenums.h',
  'gtkeventbox.h',
  'gtkeventcontroller.h',
  'gtkeventcontrollerkey.h',
  'gtkeventcontrollermotion.h',
  'gtkeventcontrollerscroll.h',
  'gtkexpander.h',
  'gtkfilechooser.h',
  'gtkfilechooserbutton.h',
  'gtkfilechooserdialog.h',
  'gtkfilechoosernative.h',
  'gtkfilechooserwidget.h',
  'gtkfilefilter.h',
  'gtkfixed.h',
  'gtkflowbox.h',
  'gtkfontbutton.h',
  'gtkfontchooser.h',
  'gtkfontchooserdialog.h',
  'gtkfontchooserwidget.h',
  'gtkframe.h',
  'gtkgesture.h',
  'gtkgesturedrag.h',
  'gtkgesturelongpress.h',
  'gtkgesturemultipress.h',
  'gtkgesturepan.h',
  'gtkgesturerotate.h',
  'gtkgesturesingle.h',
  'gtkgesturestylus.h',
  'gtkgestureswipe.h',
  'gtkgesturezoom.h',
  'gtkglarea.h',
  'gtkgrid.h',
  'gtkheaderbar.h',
  'gtkicontheme.h',
  'gtkiconview.h',
  'gtkimage.h',
  'gtkimcontext.h',
  'gtkimcontextinfo.h',
  'gtkimcontextsimple.h',
  'gtkimmodule.h',
  'gtkimmulticontext.h',
  'gtkinfobar.h',
  'gtkinvisible.h',
  'gtklabel.h',
  'gtklayout.h',
  'gtklevelbar.h',
  'gtklinkbutton.h',
  'gtklistbox.h',
  'gtkliststore.h',
  'gtklockbutton.h',
  'gtkmain.h',
  'gtkmenu.h',
  'gtkmenubar.h',
  'gtkmenubutton.h',
  'gtkmenuitem.h',
  'gtkmenushell.h',
  'gtkmenutoolbutton.h',
  'gtkmessagedialog.h',
  'gtkmodelbutton.h',
  'gtkmodules.h',
  'gtkmountoperation.h',
  'gtknativedialog.h',
  'gtknotebook.h',
  'gtkoffscreenwindow.h',
  'gtkorientable.h',
  'gtkoverlay.h',
  'gtkpadcontroller.h',
  'gtkpagesetup.h',
  'gtkpaned.h',
  'gtkpapersize.h',
  'gtkplacessidebar.h',
  'gtkplug.h',
  'gtkpopover.h',
  'gtkpopovermenu.h',
  'gtkprintcontext.h',
  'gtkprintoperation.h',
  'gtkprintoperationpreview.h',
  'gtkprintsettings.h',
  'gtkprogressbar.h',
  'gtkradiobutton.h',
  'gtkradiomenuitem.h',
  'gtkradiotoolbutton.h',
  'gtkrange.h',
  'gtkrecentchooser.h',
  'gtkrecentchooserdialog.h',
  'gtkrecentchoosermenu.h',
  'gtkrecentchooserwidget.h',
  'gtkrecentfilter.h',
  'gtkrecentmanager.h',
  'gtkrender.h',
  'gtkrevealer.h',
  'gtkscale.h',
  'gtkscalebutton.h',
  'gtkscrollable.h',
  'gtkscrollbar.h',
  'gtkscrolledwindow.h',
  'gtksearchbar.h',
  'gtksearchentry.h',
  'gtkselection.h',
  'gtkseparator.h',
  'gtkseparatormenuitem.h',
  'gtkseparatortoolitem.h',
  'gtksettings.h',
  'gtkshortcutlabel.h',
  'gtkshortcutsgroup.h',
  'gtkshortcutssection.h',
  'gtkshortcutsshortcut.h',
  'gtkshortcutswindow.h',
  'gtkshow.h',
  'gtkstacksidebar.h',
  'gtksizegroup.h',
  'gtksizerequest.h',
  'gtksocket.h',
  'gtkspinbutton.h',
  'gtkspinner.h',
  'gtkstack.h',
  'gtkstackswitcher.h',
  'gtkstatusbar.h',
  'gtkstylecontext.h',
  'gtkstyleprovider.h',
  'gtkswitch.h',
  'gtktestutils.h',
  'gtktextattributes.h',
  'gtktextbuffer.h',
  'gtktextbufferrichtext.h',
  'gtktextchild.h',
  'gtktextiter.h',
  'gtktextmark.h',
  'gtktexttag.h',
  'gtktexttagtable.h',
  'gtktextview.h',
  'gtktogglebutton.h',
  'gtktoggletoolbutton.h',
  'gtktoolbar.h',
  'gtktoolbutton.h',
  'gtktoolitem.h',
  'gtktoolitemgroup.h',
  'gtktoolpalette.h',
  'gtktoolshell.h',
  'gtktooltip.h',
  'gtktreednd.h',
  'gtktreemodel.h',
  'gtktreemodelfilter.h',
  'gtktreemodelsort.h',
  'gtktreeselection.h',
  'gtktreesortable.h',
  'gtktreestore.h',
  'gtktreeview.h',
  'gtktreeviewcolumn.h',
  'gtktypes.h',
  'gtkviewport.h',
  'gtkvolumebutton.h',
  'gtkwidget.h',
  'gtkwidgetpath.h',
  'gtkwindow.h',
  'gtkwindowgroup.h',
)

gtk_nogir_public_headers = files(
  'gtkx.h',
  'gtktextdisplay.h',
)

# Installed headers without compatibility guarantees that are not included in gtk/gtk.h
gtk_semi_private_headers = files('gtktextlayout.h')
gtk_public_headers = gtk_gir_public_headers + gtk_nogir_public_headers
install_headers(gtk_public_headers + gtk_semi_private_headers, subdir: 'gtk-3.0/gtk')

if os_unix
  gtk_unix_print_headers = [
    'gtkpagesetupunixdialog.h',
    'gtkprintunixdialog.h',
    'gtkprinter.h',
    'gtkprintjob.h',
    'gtkunixprint-autocleanups.h',
    'gtkunixprint.h',
  ]
  install_headers(gtk_unix_print_headers, subdir: 'gtk-3.0/unix-print/gtk')
endif

gtk_unix_sources = files(
  'gtkcustompaperunixdialog.c',
  'gtkpagesetupunixdialog.c',
  'gtkprinter.c',
  'gtkprinteroption.c',
  'gtkprinteroptionset.c',
  'gtkprinteroptionwidget.c',
  'gtkprintjob.c',
  'gtkprintoperation-unix.c',
  'gtkprintoperation-portal.c',
  'gtkprintunixdialog.c',
  'gtkprintbackend.c',
  'gtksearchenginetracker.c',
)

if os_unix and tracker3_enabled
  gtk_unix_sources += 'gtksearchenginetracker3.c'
endif

if os_unix
  gtk_unix_sources += 'filetransferportal.c'
endif

if os_unix
  gtk_sources += gtk_unix_sources
endif

gtk_win32_sources = files(
  'gtkprint-win32.c',
	'gtkprintoperation-win32.c',
	'gtkfilechoosernativewin32.c',
	'gtkwin32.c',
)

if os_win32
  # FIXME these sources should not be added to generate_gir?
  gtk_sources += gtk_win32_sources
endif

gen_gtk_gresources_xml = find_program('gen-gtk-gresources-xml.py')
gtk_gresources_xml = configure_file(output: 'gtk.gresources.xml',
                                    command: [
                                      gen_gtk_gresources_xml,
                                      meson.current_source_dir(),
                                      '@OUTPUT@'
                                    ])

# Re-build the theme files if sassc is available
theme_deps = []
sassc = find_program('sassc', required: false)
if sassc.found()
  sassc_opts = [ '-a', '-M', '-t', 'compact' ]

  subdir('theme/Adwaita')
  subdir('theme/HighContrast')
endif

gtkresources = gnome.compile_resources('gtkresources',
                                       gtk_gresources_xml,
                                       dependencies: theme_deps,
                                       source_dir: [
                                         # List in order of preference
                                         meson.current_build_dir(),
                                         meson.current_source_dir(),
                                       ],
                                       c_name: '_gtk',
                                       extra_args: '--manual-register')

gtk_use_x11_sources = files([
  'gtkplug.c',
  'gtksocket.c',
  'gtkxembed.c',
  'deprecated/gtktrayicon-x11.c',
  'gtkapplication-x11.c',
  'gtkmountoperation-x11.c',
])

gtk_use_wayland_sources = files([
  'gtkapplication-wayland.c'
])

if quartz_enabled
  gtk_sources += files(
    'gtkclipboard-quartz.c',
    'gtkdnd-quartz.c',
  )
else
  gtk_sources += files(
    'gtkclipboard.c',
    'gtkdnd.c',
    'gtkdragdest.c',
  )
endif

gtk_use_wayland_or_x11_sources = files( 'gtkapplication-dbus.c')

gtk_use_win32_sources = files(
	'gtkwin32embed.c',
	'gtkwin32embedwidget.c',
	'gtkmountoperation-stub.c',
)

gtk_use_quartz_sources = files(
  'gtksearchenginequartz.c',
  'gtkmountoperation-stub.c',
  'gtkapplication-quartz.c',
  'gtkapplication-quartz-menu.c',
  'gtkfilechoosernativequartz.c',
  'gtkquartz.c',
)

gtk_stub_sources = files('gtkmountoperation-stub.c')

gtk_dbus_src = gnome.gdbus_codegen('gtkdbusgenerated', 'gtkdbusinterfaces.xml',
                                   interface_prefix: 'org.Gtk.',
                                   namespace: '_Gtk')

gtkmarshalers = gnome.genmarshal('gtkmarshalers',
                                 sources: 'gtkmarshalers.list',
                                 prefix: '_gtk_marshal',
                                 valist_marshallers: true)

gtkmarshal_h = gtkmarshalers[1]

gtktypebuiltins = gnome.mkenums('gtktypebuiltins',
                                sources: gtk_public_headers + gtk_deprecated_headers + a11y_headers,
                                c_template: 'gtktypebuiltins.c.template',
                                h_template: 'gtktypebuiltins.h.template',
                                install_dir: join_paths(gtk_includedir, 'gtk-3.0/gtk'),
                                install_header: true)

gtktypebuiltins_h = gtktypebuiltins[1]

gtkprivatetypebuiltins = gnome.mkenums('gtkprivatetypebuiltins',
                                       sources: gtk_private_type_headers,
                                       c_template: 'gtkprivatetypebuiltins.c.template',
                                       h_template: 'gtkprivatetypebuiltins.h.template')

gtkprivatetypebuiltins_h = gtkprivatetypebuiltins[1]

# Generate gtktypefuncs.inc
typefuncs = custom_target('gtktypefuncs.inc',
                          depends: [ gdkenum_h ],
                          output: 'gtktypefuncs.inc',
                          input: gdk_public_headers +
                            gtk_public_headers +
                            gtk_deprecated_headers +
                            [ gtktypebuiltins_h, gdkenum_h ],
                          command: [
                            find_program('gentypefuncs.py'),
                            '@OUTPUT@',
                            '@INPUT@',
                          ],
                          install: false)


gtkversion_cdata = configuration_data()
gtkversion_cdata.set('GTK_MAJOR_VERSION', gtk_major_version)
gtkversion_cdata.set('GTK_MINOR_VERSION', gtk_minor_version)
gtkversion_cdata.set('GTK_MICRO_VERSION', gtk_micro_version)
gtkversion_cdata.set('GTK_BINARY_AGE', gtk_binary_age)
gtkversion_cdata.set('GTK_INTERFACE_AGE', gtk_interface_age)
gtkversion_cdata.set('GTK_API_VERSION', gtk_api_version)
gtkversion_cdata.set('GTK_BINARY_VERSION', gtk_binary_version)
gtkversion_cdata.set('GTK_VERSION', meson.project_version())
gtkversion_cdata.set('EXE_MANIFEST_ARCHITECTURE', '*')
gtkversion_cdata.set('LT_CURRENT_MINUS_AGE', gtk_soversion)

gtkversion = configure_file(input: 'gtkversion.h.in',
                            output: 'gtkversion.h',
                            configuration: gtkversion_cdata,
                            install_dir: join_paths(gtk_includedir, 'gtk-3.0/gtk'))

gtk_gen_headers = [
  gtkmarshalers,
  gtkprivatetypebuiltins,
  gtktypebuiltins,
  gtkversion,
]

gtk_sources += [
  gtk_dbus_src,
  gtk_deprecated_sources,
  inspector_sources,
  a11y_sources,
  gtkresources,
  gtkmarshalers,
  gtkprivatetypebuiltins,
  gtktypebuiltins,
]

gtk_deps = [
  gmodule_dep,
  glib_dep,
  gobject_dep,
  platform_gio_dep,
  pangocairo_dep,
  pango_dep,
  pangoft_dep,
  harfbuzz_dep,
  fribidi_dep,
  cairogobj_dep,
  cairo_dep,
  fontconfig_dep,
  pixbuf_dep,
  atk_dep,
  epoxy_dep,
  libm,
]

if x11_enabled
  x11_data_prefix = dependency('x11').get_variable(pkgconfig: 'prefix')

  gtk_cargs += [ '-DX11_DATA_PREFIX="@0@"'.format(x11_data_prefix), ]
  gtk_sources += gtk_use_x11_sources
  gtk_deps += [ xi_dep, x11_dep, ]
endif

if wayland_enabled
  gtk_sources += gtk_use_wayland_sources
endif

if win32_enabled
  gtk_cargs += []
  gtk_sources += gtk_use_win32_sources
  gtk_deps += [ giowin32_dep, pangowin32_dep ]

  gtk_rc_body = configure_file(
    input: 'gtk-win32.rc.body.in',
    output: 'gtk-win32.rc.body',
	configuration: gtkversion_cdata,
  )
  gtk_rc_manifest = configure_file(
    input: 'libgtk3.manifest.in',
    output: 'libgtk3.manifest',
    configuration: gtkversion_cdata,
  )

  # Unfortunately, an extra directive in the .rc file is required for earlier
  # Visual Studio for embedding manifests via .rc files (2010 [v16] and earlier)
  # using ISOLATIONAWARE_MANIFEST_RESOURCE_ID to really work.  Somehow for
  # Visual Studio 2008 builds, this does not yet work.
  old_msvc = false

  if cc.get_id() == 'msvc' and cc.version().split('.')[0].to_int() < 17
    old_msvc = true
  endif

  gtk_rc = custom_target(
             'gtk.rc',
             output: 'gtk.rc',
             input: gtk_rc_body,
             command: [find_program('gen-rc.py'),
               '@OUTPUT@',
               '@INPUT@',
               old_msvc ? '1' : ''
             ],
             install: false,
           )
  gtk_res = import('windows').compile_resources(gtk_rc)
  gtk_sources += gtk_res
else
  gtk_deps += [ atkbridge_dep, ]
endif

if quartz_enabled
  gtk_sources += gtk_use_quartz_sources
  gtk_cargs += ['-xobjective-c']
endif

# So we don't add these twice
if x11_enabled or wayland_enabled
  gtk_sources += gtk_use_wayland_or_x11_sources
  gtk_deps += pangoft_dep
endif

if not (x11_enabled or win32_enabled or quartz_enabled)
  gtk_sources += gtk_stub_sources
endif

if cloudproviders_enabled
  gtk_deps += cloudproviders_dep
endif

if os_unix and tracker3_enabled
  gtk_deps += tracker3_dep
endif

# Unconditional. If libintl isn't found,
# the object just does nothing being in the deplist
gtk_deps += libintl_dep

gtk_settings_schemas = [
  'org.gtk.Settings.FileChooser.gschema.xml',
  'org.gtk.Settings.ColorChooser.gschema.xml',
  'org.gtk.Settings.EmojiChooser.gschema.xml',
  'org.gtk.Settings.Debug.gschema.xml',
]
install_data(gtk_settings_schemas, install_dir: gtk_schemasdir)
gnome.compile_schemas(depend_files: files(gtk_settings_schemas),
                      build_by_default: true)
gtk_schema_build_dir = meson.current_build_dir()

# Check for more things
if cc.has_header('langinfo.h')
  foreach nl_enum: [ '_NL_MEASUREMENT_MEASUREMENT',
                     '_NL_PAPER_HEIGHT',
                     '_NL_PAPER_WIDTH',
                     '_NL_TIME_FIRST_WEEKDAY', ]
    cdata.set('HAVE_' + nl_enum, cc.has_header_symbol('langinfo.h', nl_enum) ? 1 : false)
  endforeach
endif

gtk_dep_sources = [gtkversion, gtktypebuiltins_h]

# Static immodules
gtk_included_im_deps = []
foreach l: immodules
  name = l[0]
  sources = l[1]
  cond = l.get(2, true)
  cflags = l.get(3, [])

  if cond and (builtin_immodules.contains(name) or builtin_all_immodules)
    gtk_cargs += ['-DINCLUDE_IM_@0@'.format(name.underscorify())]
    mod = static_library('staticimmodule-@0@'.format(name),
                         sources + gtk_dep_sources,
                         dependencies: gtk_deps + [libgdk_dep] ,
                         c_args: gtk_cargs + common_cflags + cflags,
                         include_directories: [confinc, gdkinc, gtkinc])

    gtk_included_im_deps += declare_dependency(link_with: mod)
  endif
endforeach

# Library
libgtk = library('gtk-3',
                 soversion: gtk_soversion,
                 version: gtk_library_version,
                 sources: [typefuncs, gtk_sources, gtkmarshal_h, gtkprivatetypebuiltins_h],
                 c_args: gtk_cargs + common_cflags,
                 include_directories: [confinc, gdkinc, gtkinc],
                 dependencies: gtk_deps + [libgdk_dep] + gtk_included_im_deps,
                 link_with: libgdk,
                 link_args: common_ldflags,
                 darwin_versions : gtk_osxversions,
                 install: true)

# Introspection
if build_gir
  gir_args = ['--quiet']

  gtk_introspection_sources = [
    gtk_gir_public_headers,
    gtk_deprecated_headers,
    gtk_sources,
    a11y_headers,
    a11y_sources,
    gtktypebuiltins_h,
    gtkversion,
  ]

  gtk_gir_includes = [gdk_gir[0], 'Atk-1.0']
  gtk_gir_extra_args = gir_args + ['-DGTK_COMPILATION',
                                   '--c-include=gtk/gtk-a11y.h']
  if x11_enabled
    gtk_gir_includes += ['xlib-2.0']
    gtk_gir_extra_args += ['--c-include=gtk/gtkx.h']
  endif

  gtk_gir = gnome.generate_gir(libgtk,
                               sources: gtk_introspection_sources,
                               namespace: 'Gtk',
                               nsversion: gtk_api_version,
                               identifier_prefix: 'Gtk',
                               symbol_prefix: 'gtk',
                               export_packages: 'gtk+-3.0',
                               includes: gtk_gir_includes,
                               header: 'gtk/gtk.h',
                               install: true,
                               dependencies: gdk_gir_dep,
                               extra_args: gtk_gir_extra_args)
  gtk_dep_sources += gtk_gir
endif

libgtk_dep = declare_dependency(sources: gtk_dep_sources,
                                include_directories: [confinc, gtkinc],
                                dependencies: gtk_deps + [libgdk_dep],
                                link_with: libgtk,
                                link_args: common_ldflags)

foreach lang : [
    'bn',
    'de',
    'da',
    'fr',
    'es',
    'et',
    'fi',
    'hi',
    'hu',
    'it',
    'ja',
    'ko',
    'lt',
    'ms',
    'nb',
    'nl',
    'pl',
    'pt',
    'ru',
    'sv',
    'th',
    'uk',
    'zh'
  ]
  conf = configuration_data()
  conf.set('lang', lang)
  resxml = configure_file(input: 'emoji/gresource.xml.in',
                          output: lang + '.gresource.xml',
                          configuration: conf
  )

  gnome.compile_resources(lang,
    resxml,
    source_dir: 'emoji',
    gresource_bundle: true,
    install: true,
    install_dir: join_paths(gtk_datadir, 'gtk-3.0', 'emoji'),
  )
endforeach


if quartz_enabled
  install_data(['gtk-keys.css.mac'],
               install_dir:  join_paths(get_option('datadir'), 'themes/Mac/gtk-3.0'),
               rename: ['gtk-keys.css'])
endif

install_data(['gtk-keys.css.default'],
             install_dir:  join_paths(get_option('datadir'), 'themes/Default/gtk-3.0'),
             rename: ['gtk-keys.css'])

install_data(['gtk-keys.css.emacs'],
             install_dir:  join_paths(get_option('datadir'), 'themes/Emacs/gtk-3.0'),
             rename: ['gtk-keys.css'])

# Data to install
install_data('gtkbuilder.rng',
             install_dir: join_paths(gtk_datadir, 'gtk-3.0'))

install_data('gtkbuilder.loc', 'gtkbuilder.its',
             install_dir: join_paths(gtk_datadir, 'gettext/its'))

####### Utils ######
gtk_tools = []

gtk_query_settings = executable(
  'gtk-query-settings',
  'gtk-query-settings.c',
  c_args: gtk_cargs,
  dependencies: libgtk_dep,
  install: true
)
gtk_tools += gtk_query_settings

gtk_builder_tool = executable(
  'gtk-builder-tool',
  'gtk-builder-tool.c',
  c_args: gtk_cargs,
  dependencies: libgtk_dep,
  install: true
)
gtk_tools += gtk_builder_tool

extra_update_icon_cache_objs = []
if win32_enabled
  gen_uac_manifest = find_program('generate-uac-manifest.py')

  uac_exe_pkg = 'gtk3'
  uac_exe_name = 'gtk-update-icon-cache'

  # Well, we have to forgo the xxx.exe.manifest in the output listing, since
  # compile_resources doesn't like to consume targets with multiple outputs,
  # and the xxx.exe.manifest and xxx.rc are tied together
  uac_rc = custom_target(
    'gtk/@0@.rc'.format(uac_exe_name),
    output: ['@0@.rc'.format(uac_exe_name)],
    command: [gen_uac_manifest,
              '-p=@0@'.format(uac_exe_pkg),
              '-n=@0@'.format(uac_exe_name),
              '--pkg-version=@0@'.format(meson.project_version()),
              '--output-dir=@OUTDIR@'],
  )
  extra_update_icon_cache_objs = import('windows').compile_resources(uac_rc)
endif

gtk_update_icon_cache = executable(
  'gtk-update-icon-cache',
  'updateiconcache.c',
  extra_update_icon_cache_objs,
  c_args: gtk_cargs,
  dependencies: libgtk_dep,
  install: true
)
gtk_tools += gtk_update_icon_cache

gtk_query_immodules = executable(
  'gtk-query-immodules-3.0',
  'queryimmodules.c',
  'gtkutils.c',
  c_args: gtk_cargs,
  dependencies: libgtk_dep,
  install: true
)
gtk_tools += gtk_query_immodules

gtk_encode_symbolic_svg = executable(
  'gtk-encode-symbolic-svg',
  'encodesymbolic.c',
  c_args: gtk_cargs,
  dependencies: libgtk_dep,
  install: true
)
gtk_tools += gtk_encode_symbolic_svg

gtk_launch = executable(
  'gtk-launch',
  'gtk-launch.c',
  c_args: gtk_cargs,
  dependencies: libgtk_dep,
  install: true
)
gtk_tools += gtk_launch

foreach tool: gtk_tools
  meson.override_find_program(tool.name(), tool)
endforeach
