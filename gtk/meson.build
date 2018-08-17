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
]

# List of sources that do not contain public API, and should not be
# introspected
gtk_private_sources = files([
  'fallback-c89.c',
  'fnmatch.c',
  'tools/gdkpixbufutils.c',
  'gtkactionhelper.c',
  'gtkactionmuxer.c',
  'gtkactionobservable.c',
  'gtkactionobserver.c',
  'gtkallocatedbitmask.c',
  'gtkapplicationaccels.c',
  'gtkapplicationimpl.c',
  'gtkbookmarksmanager.c',
  'gtkbuilder-menus.c',
  'gtkbuilderparser.c',
  'gtkcellareaboxcontext.c',
  'gtkcoloreditor.c',
  'gtkcolorplane.c',
  'gtkcolorpicker.c',
  'gtkcolorpickerkwin.c',
  'gtkcolorpickerportal.c',
  'gtkcolorpickershell.c',
  'gtkcolorscale.c',
  'gtkcolorswatch.c',
  'gtkcssanimatedstyle.c',
  'gtkcssanimation.c',
  'gtkcssarrayvalue.c',
  'gtkcssbgsizevalue.c',
  'gtkcssbordervalue.c',
  'gtkcsscalcvalue.c',
  'gtkcsscolorvalue.c',
  'gtkcsscornervalue.c',
  'gtkcssdimensionvalue.c',
  'gtkcssdynamic.c',
  'gtkcsseasevalue.c',
  'gtkcssenumvalue.c',
  'gtkcssfiltervalue.c',
  'gtkcssfontfeaturesvalue.c',
  'gtkcssfontvariationsvalue.c',
  'gtkcssiconthemevalue.c',
  'gtkcssimage.c',
  'gtkcssimagebuiltin.c',
  'gtkcssimagecrossfade.c',
  'gtkcssimagefallback.c',
  'gtkcssimageicontheme.c',
  'gtkcssimageinvalid.c',
  'gtkcssimagelinear.c',
  'gtkcssimagepaintable.c',
  'gtkcssimageradial.c',
  'gtkcssimagerecolor.c',
  'gtkcssimagescaled.c',
  'gtkcssimageurl.c',
  'gtkcssimagevalue.c',
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
  'gtkcssrepeatvalue.c',
  'gtkcssrgbavalue.c',
  'gtkcssselector.c',
  'gtkcssshadowsvalue.c',
  'gtkcssshadowvalue.c',
  'gtkcssshorthandproperty.c',
  'gtkcssshorthandpropertyimpl.c',
  'gtkcssstaticstyle.c',
  'gtkcssstringvalue.c',
  'gtkcssstyle.c',
  'gtkcssstylechange.c',
  'gtkcssstyleproperty.c',
  'gtkcssstylepropertyimpl.c',
  'gtkcsstransformvalue.c',
  'gtkcsstransientnode.c',
  'gtkcsstransition.c',
  'gtkcsstypes.c',
  'gtkcssunsetvalue.c',
  'gtkcssvalue.c',
  'gtkcsswidgetnode.c',
  'gtkcustomlayout.c',
  'gtkfilechooserembed.c',
  'gtkfilechooserentry.c',
  'gtkfilechoosererrorstack.c',
  'gtkfilechoosernativeportal.c',
  'gtkfilechooserutils.c',
  'gtkfilesystem.c',
  'gtkfilesystemmodel.c',
  'gtkgizmo.c',
  'gtkgladecatalog.c',
  'gtkhsla.c',
  'gtkicon.c',
  'gtkiconcache.c',
  'tools/gtkiconcachevalidator.c',
  'gtkiconhelper.c',
  'gtkkineticscrolling.c',
  'gtkkeyhash.c',
  'gtkmagnifier.c',
  'gtkmenusectionbox.c',
  'gtkmenutracker.c',
  'gtkmenutrackeritem.c',
  'gtkpango.c',
  'gskpango.c',
  'gtkpathbar.c',
  'gtkplacessidebar.c',
  'gtkplacesview.c',
  'gtkplacesviewrow.c',
  'gtkpointerfocus.c',
  'gtkprintutils.c',
  'gtkprivate.c',
  'gtkprogresstracker.c',
  'gtkrbtree.c',
  'gtkquery.c',
  'gtkscaler.c',
  'gtksearchengine.c',
  'gtksearchenginemodel.c',
  'gtksearchenginesimple.c',
  'gtksizerequestcache.c',
  'gtkstyleanimation.c',
  'gtkstylecascade.c',
  'gtkstyleproperty.c',
  'gtktextbtree.c',
  'gtktrashmonitor.c',
  'gtktreedatalist.c',
])

# List of files that contain public API, and should be introspected
gtk_public_sources = files([
  'gtkaboutdialog.c',
  'gtkaccelgroup.c',
  'gtkaccellabel.c',
  'gtkaccessible.c',
  'gtkactionable.c',
  'gtkactionbar.c',
  'gtkadjustment.c',
  'gtkappchooser.c',
  'gtkappchooserbutton.c',
  'gtkappchooserdialog.c',
  'gtkappchooserwidget.c',
  'gtkapplication.c',
  'gtkapplicationwindow.c',
  'gtkaspectframe.c',
  'gtkassistant.c',
  'gtkbin.c',
  'gtkbinlayout.c',
  'gtkborder.c',
  'gtkboxlayout.c',
  'gtkbox.c',
  'gtkbuildable.c',
  'gtkbuilder.c',
  'gtkbutton.c',
  'gtkcalendar.c',
  'gtkcellarea.c',
  'gtkcellareabox.c',
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
  'gtkcenterbox.c',
  'gtkcheckbutton.c',
  'gtkcheckmenuitem.c',
  'gtkcolorbutton.c',
  'gtkcolorchooser.c',
  'gtkcolorchooserdialog.c',
  'gtkcolorchooserwidget.c',
  'gtkcolorutils.c',
  'gtkcombobox.c',
  'gtkcomboboxtext.c',
  'gtkcomposetable.c',
  'gtkcontainer.c',
  'gtkcssprovider.c',
  'gtkdialog.c',
  'gtkdnd.c',
  'gtkdragdest.c',
  'gtkdragsource.c',
  'gtkdrawingarea.c',
  'gtkeditable.c',
  'gtkemojichooser.c',
  'gtkemojicompletion.c',
  'gtkentry.c',
  'gtkentrybuffer.c',
  'gtkentrycompletion.c',
  'gtkeventcontroller.c',
  'gtkeventcontrollerkey.c',
  'gtkeventcontrollerlegacy.c',
  'gtkeventcontrollermotion.c',
  'gtkeventcontrollerscroll.c',
  'gtkexpander.c',
  'gtkfilechooser.c',
  'gtkfilechooserbutton.c',
  'gtkfilechooserdialog.c',
  'gtkfilechoosernative.c',
  'gtkfilechooserwidget.c',
  'gtkfilefilter.c',
  'gtkfilterlistmodel.c',
  'gtkfixed.c',
  'gtkfixedlayout.c',
  'gtkflattenlistmodel.c',
  'gtkflowbox.c',
  'gtkfontbutton.c',
  'gtkfontchooser.c',
  'gtkfontchooserdialog.c',
  'gtkfontchooserutils.c',
  'gtkfontchooserwidget.c',
  'gtkframe.c',
  'gtkgesture.c',
  'gtkgesturedrag.c',
  'gtkgesturelongpress.c',
  'gtkgestureclick.c',
  'gtkgesturepan.c',
  'gtkgesturerotate.c',
  'gtkgesturesingle.c',
  'gtkgesturestylus.c',
  'gtkgestureswipe.c',
  'gtkgesturezoom.c',
  'gtkglarea.c',
  'gtkgrid.c',
  'gtkgridlayout.c',
  'gtkheaderbar.c',
  'gtkicontheme.c',
  'gtkiconview.c',
  'gtkimage.c',
  'gtkimagedefinition.c',
  'gtkimcontext.c',
  'gtkimcontextsimple.c',
  'gtkimmodule.c',
  'gtkimmulticontext.c',
  'gtkinfobar.c',
  'gtklabel.c',
  'gtklayoutchild.c',
  'gtklayoutmanager.c',
  'gtklevelbar.c',
  'gtklinkbutton.c',
  'gtklistbox.c',
  'gtklistlistmodel.c',
  'gtkliststore.c',
  'gtklockbutton.c',
  'gtkmain.c',
  'gtkmaplistmodel.c',
  'gtkmediacontrols.c',
  'gtkmediafile.c',
  'gtkmediastream.c',
  'gtkmenu.c',
  'gtkmenubar.c',
  'gtkmenubutton.c',
  'gtkmenuitem.c',
  'gtkmenushell.c',
  'gtkmenutoolbutton.c',
  'gtkmessagedialog.c',
  'gtkmodelbutton.c',
  'gtkmodelmenuitem.c',
  'gtkmodules.c',
  'gtkmountoperation.c',
  'gtknativedialog.c',
  'gtknomediafile.c',
  'gtknotebook.c',
  'gtkorientable.c',
  'gtkoverlay.c',
  'gtkoverlaylayout.c',
  'gtkpadcontroller.c',
  'gtkpagesetup.c',
  'gtkpaned.c',
  'gtkpapersize.c',
  'gtkpasswordentry.c',
  'gtkpicture.c',
  'gtkpopover.c',
  'gtkpopovermenu.c',
  'gtkpopovermenubar.c',
  'gtkprintcontext.c',
  'gtkprintoperation.c',
  'gtkprintoperationpreview.c',
  'gtkprintsettings.c',
  'gtkprogressbar.c',
  'gtkpropertylookuplistmodel.c',
  'gtkradiobutton.c',
  'gtkradiomenuitem.c',
  'gtkradiotoolbutton.c',
  'gtkrange.c',
  'gtktreerbtree.c',
  'gtkrecentmanager.c',
  'gtkrender.c',
  'gtkrenderbackground.c',
  'gtkrenderborder.c',
  'gtkrendericon.c',
  'gtkrendernodepaintable.c',
  'gtkrevealer.c',
  'gtkroot.c',
  'gtknative.c',
  'gtkroundedbox.c',
  'gtkscale.c',
  'gtkscalebutton.c',
  'gtkscrollable.c',
  'gtkscrollbar.c',
  'gtkscrolledwindow.c',
  'gtksearchbar.c',
  'gtksearchentry.c',
  'gtkselection.c',
  'gtkselectionmodel.c',
  'gtkseparator.c',
  'gtkseparatormenuitem.c',
  'gtkseparatortoolitem.c',
  'gtksettings.c',
  'gtkshortcut.c',
  'gtkshortcutcontroller.c',
  'gtkshortcutlabel.c',
  'gtkshortcutmanager.c',
  'gtkshortcutsgroup.c',
  'gtkshortcutssection.c',
  'gtkshortcutsshortcut.c',
  'gtkshortcutswindow.c',
  'gtkshortcuttrigger.c',
  'gtkshow.c',
  'gtksidebarrow.c',
  'gtksingleselection.c',
  'gtksizegroup.c',
  'gtksizerequest.c',
  'gtkslicelistmodel.c',
  'gtksnapshot.c',
  'gtksortlistmodel.c',
  'gtkspinbutton.c',
  'gtkspinner.c',
  'gtkstack.c',
  'gtkstacksidebar.c',
  'gtkstackswitcher.c',
  'gtkstatusbar.c',
  'gtkstylecontext.c',
  'gtkstyleprovider.c',
  'gtkswitch.c',
  'gtktestutils.c',
  'gtktext.c',
  'gtktextattributes.c',
  'gtktextbuffer.c',
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
  'gtktoolshell.c',
  'gtktooltip.c',
  'gtktooltipwindow.c',
  'gtktreednd.c',
  'gtktreelistmodel.c',
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
  'gtkvideo.c',
  'gtkviewport.c',
  'gtkvolumebutton.c',
  'gtkwidget.c',
  'gtkwidgetfocus.c',
  'gtkwidgetpaintable.c',
  'gtkwidgetpath.c',
  'gtkwindow.c',
  'gtkwindowgroup.c',
])

gtk_private_type_headers = files([
  'gtkcsstypesprivate.h',
  'gtktexthandleprivate.h',
])

gtk_public_headers = files([
  'gtkaboutdialog.h',
  'gtkaccelgroup.h',
  'gtkaccellabel.h',
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
  'gtkbin.h',
  'gtkbinlayout.h',
  'gtkborder.h',
  'gtkbox.h',
  'gtkboxlayout.h',
  'gtkbuildable.h',
  'gtkbuilder.h',
  'gtkbutton.h',
  'gtkcalendar.h',
  'gtkcenterbox.h',
  'gtkcellarea.h',
  'gtkcellareabox.h',
  'gtkcellareacontext.h',
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
  'gtkcolorbutton.h',
  'gtkcolorchooser.h',
  'gtkcolorchooserdialog.h',
  'gtkcolorchooserwidget.h',
  'gtkcolorutils.h',
  'gtkcombobox.h',
  'gtkcomboboxtext.h',
  'gtkcontainer.h',
  'gtkcssprovider.h',
  'gtkcustomlayout.h',
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
  'gtkeventcontroller.h',
  'gtkeventcontrollerkey.h',
  'gtkeventcontrollerscroll.h',
  'gtkeventcontrollermotion.h',
  'gtkeventcontrollerlegacy.h',
  'gtkexpander.h',
  'gtkfilechooser.h',
  'gtkfilechooserbutton.h',
  'gtkfilechooserdialog.h',
  'gtkfilechoosernative.h',
  'gtkfilechooserwidget.h',
  'gtkfilefilter.h',
  'gtkfilterlistmodel.h',
  'gtkfixed.h',
  'gtkfixedlayout.h',
  'gtkflattenlistmodel.h',
  'gtkflowbox.h',
  'gtkfontbutton.h',
  'gtkfontchooser.h',
  'gtkfontchooserdialog.h',
  'gtkfontchooserwidget.h',
  'gtkframe.h',
  'gtkgesture.h',
  'gtkgesturedrag.h',
  'gtkgesturelongpress.h',
  'gtkgestureclick.h',
  'gtkgesturepan.h',
  'gtkgesturerotate.h',
  'gtkgesturesingle.h',
  'gtkgesturestylus.h',
  'gtkgestureswipe.h',
  'gtkgesturezoom.h',
  'gtkglarea.h',
  'gtkgrid.h',
  'gtkgridlayout.h',
  'gtkheaderbar.h',
  'gtkicontheme.h',
  'gtkiconview.h',
  'gtkimage.h',
  'gtkimcontext.h',
  'gtkimcontextsimple.h',
  'gtkimmodule.h',
  'gtkimmulticontext.h',
  'gtkinfobar.h',
  'gtklabel.h',
  'gtklayoutchild.h',
  'gtklayoutmanager.h',
  'gtklevelbar.h',
  'gtklinkbutton.h',
  'gtklistbox.h',
  'gtkliststore.h',
  'gtklockbutton.h',
  'gtkmain.h',
  'gtkmaplistmodel.h',
  'gtkmediacontrols.h',
  'gtkmediafile.h',
  'gtkmediastream.h',
  'gtkmenu.h',
  'gtkmenubar.h',
  'gtkmenubutton.h',
  'gtkmenuitem.h',
  'gtkmenushell.h',
  'gtkmenutoolbutton.h',
  'gtkmessagedialog.h',
  'gtkmodelbutton.h',
  'gtkmountoperation.h',
  'gtknative.h',
  'gtknativedialog.h',
  'gtknotebook.h',
  'gtkorientable.h',
  'gtkoverlay.h',
  'gtkpadcontroller.h',
  'gtkpagesetup.h',
  'gtkpaned.h',
  'gtkpapersize.h',
  'gtkpasswordentry.h',
  'gtkpicture.h',
  'gtkpopover.h',
  'gtkpopovermenu.h',
  'gtkpopovermenubar.h',
  'gtkprintcontext.h',
  'gtkprintoperation.h',
  'gtkprintoperationpreview.h',
  'gtkprintsettings.h',
  'gtkprogressbar.h',
  'gtkradiobutton.h',
  'gtkradiomenuitem.h',
  'gtkradiotoolbutton.h',
  'gtkrange.h',
  'gtkrecentmanager.h',
  'gtkrender.h',
  'gtkrevealer.h',
  'gtkroot.h',
  'gtkscale.h',
  'gtkscalebutton.h',
  'gtkscrollable.h',
  'gtkscrollbar.h',
  'gtkscrolledwindow.h',
  'gtksearchbar.h',
  'gtksearchentry.h',
  'gtkselection.h',
  'gtkselectionmodel.h',
  'gtkseparator.h',
  'gtkseparatormenuitem.h',
  'gtkseparatortoolitem.h',
  'gtksettings.h',
  'gtkshortcut.h',
  'gtkshortcutcontroller.h',
  'gtkshortcutlabel.h',
  'gtkshortcutmanager.h',
  'gtkshortcutsgroup.h',
  'gtkshortcutssection.h',
  'gtkshortcutsshortcut.h',
  'gtkshortcutswindow.h',
  'gtkshortcuttrigger.h',
  'gtkshow.h',
  'gtksingleselection.h',
  'gtksizegroup.h',
  'gtksizerequest.h',
  'gtkslicelistmodel.h',
  'gtksnapshot.h',
  'gtksortlistmodel.h',
  'gtkspinbutton.h',
  'gtkspinner.h',
  'gtkstack.h',
  'gtkstacksidebar.h',
  'gtkstackswitcher.h',
  'gtkstatusbar.h',
  'gtkstylecontext.h',
  'gtkstyleprovider.h',
  'gtkswitch.h',
  'gtktestutils.h',
  'gtktext.h',
  'gtktextbuffer.h',
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
  'gtktoolshell.h',
  'gtktooltip.h',
  'gtktreednd.h',
  'gtktreelistmodel.h',
  'gtktreemodel.h',
  'gtktreemodelfilter.h',
  'gtktreemodelsort.h',
  'gtktreeselection.h',
  'gtktreesortable.h',
  'gtktreestore.h',
  'gtktreeview.h',
  'gtktreeviewcolumn.h',
  'gtktypes.h',
  'gtkvideo.h',
  'gtkviewport.h',
  'gtkvolumebutton.h',
  'gtkwidget.h',
  'gtkwidgetpaintable.h',
  'gtkwidgetpath.h',
  'gtkwindow.h',
  'gtkwindowgroup.h',
  'gtk-a11y.h',
  'gtk-autocleanups.h',
  'gtk.h',
])

install_headers(gtk_public_headers, subdir: 'gtk-4.0/gtk/')

gtk_sources = gtk_public_sources + gtk_private_sources

proto_sources = [
  ['text-input', 'unstable', 'v3', ],
]

im_wayland_sources = files('gtkimcontextwayland.c')

foreach p: proto_sources
  proto_name = p.get(0)
  proto_stability = p.get(1)

  if wayland_enabled
    if proto_stability == 'stable'
      output_base = proto_name
      input = '@0@.xml'.format(proto_name)
    else
      proto_version = p.get(2)
      output_base = '@0@-@1@-@2@'.format(proto_name, proto_stability, proto_version)
      input = join_paths(proto_dir, '@0@/@1@/@2@.xml'.format(proto_stability, proto_name, output_base))
    endif

    # wayland_scanner is defined in gdk/wayland/meson.build
    im_wayland_sources += custom_target('@0@ client header'.format(output_base),
                                        input: input,
                                        output: '@0@-client-protocol.h'.format(output_base),
                                        command: [
                                          wayland_scanner,
                                          'client-header',
                                          '@INPUT@', '@OUTPUT@',
                                        ])

    im_wayland_sources += custom_target('@0@ source'.format(output_base),
                                        input: input,
                                        output: '@0@-protocol.c'.format(output_base),
                                        command: [
                                          wayland_scanner,
                                          'private-code',
                                          '@INPUT@', '@OUTPUT@',
                                        ])
  endif
endforeach

if os_unix
  gtk_unix_print_headers = [
    'gtkpagesetupunixdialog.h',
    'gtkprintunixdialog.h',
    'gtkprinter.h',
    'gtkprintjob.h',
    'gtkunixprint-autocleanups.h',
    'gtkunixprint.h',
  ]
  install_headers(gtk_unix_print_headers, subdir: 'gtk-4.0/unix-print/gtk')

  gtk_unix_print_sources = files([
    'gtkcustompaperunixdialog.c',
    'gtkpagesetupunixdialog.c',
    'gtkprintbackend.c',
    'gtkprinter.c',
    'gtkprinteroption.c',
    'gtkprinteroptionset.c',
    'gtkprinteroptionwidget.c',
    'gtkprintjob.c',
    'gtkprintoperation-unix.c',
    'gtkprintoperation-portal.c',
    'gtkprintunixdialog.c',
    'gtksearchenginetracker.c',
  ])

  gtk_sources += gtk_unix_print_sources
endif

if os_win32
  gtk_win32_print_sources = files([
    'gtkprint-win32.c',
    'gtkprintoperation-win32.c',
    'gtkfilechoosernativewin32.c',
    'gtkwin32.c',
  ])

  gtk_sources += gtk_win32_print_sources
  gtk_sources += ['gtkimcontextime.c']

  if cc.has_header_symbol('windows.h', 'IPrintDialogCallback')
    cdata.set('HAVE_IPRINTDIALOGCALLBACK', 1)
  endif
endif

if x11_enabled
  gtk_sources += ['gtkapplication-x11.c', 'gtkmountoperation-x11.c']
  gtk_sources += ['gtkimcontextxim.c']
endif

if wayland_enabled
  gtk_sources += ['gtkapplication-wayland.c']
  gtk_sources += im_wayland_sources
endif

if x11_enabled or wayland_enabled
  gtk_sources += ['gtkapplication-dbus.c']
endif

if win32_enabled
  gtk_sources += ['gtkmountoperation-stub.c']
endif

if quartz_enabled
  gtk_sources += files([
    'gtksearchenginequartz.c',
    'gtkmountoperation-stub.c',
    'gtkapplication-quartz.c',
    'gtkapplication-quartz-menu.c',
    'gtkfilechoosernativequartz.c',
    'gtkquartz.c',
  ])
  gtk_sources += ['gtkimcontextquartz.c']

  gtk_cargs += ['-xobjective-c']  # FIXME? maybe add_languages() instead?
endif

if not (x11_enabled or win32_enabled or quartz_enabled)
  gtk_sources += ['gtkmountoperation-stub.c', ]
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

gtk_x11_sources = files([
  'gtkapplication-x11.c',
  'gtkmountoperation-x11.c',
])

gtk_wayland_sources = files([
  'gtkapplication-wayland.c'
])

gtk_use_wayland_or_x11_c_sources = files([
  'gtkapplication-dbus.c'
])

gtk_dbus_src = gnome.gdbus_codegen('gtkdbusgenerated',
                                   'gtkdbusinterfaces.xml',
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
                                install_dir: join_paths(gtk_includedir, 'gtk-4.0/gtk'),
                                install_header: true)

gtktypebuiltins_h = gtktypebuiltins[1]

gtkprivatetypebuiltins = gnome.mkenums('gtkprivatetypebuiltins',
                                       sources: gtk_private_type_headers,
                                       c_template: 'gtkprivatetypebuiltins.c.template',
                                       h_template: 'gtkprivatetypebuiltins.h.template')

gtkprivatetypebuiltins_h = gtkprivatetypebuiltins[1]

# Generate gtktypefuncs.inc
typefuncs = custom_target('gtktypefuncs.inc',
                          depends: [ gdkenum_h, gskenum_h, ],
                          output: 'gtktypefuncs.inc',
                          input: gdk_headers +
                            gtk_public_headers +
                            gtk_deprecated_headers +
                            [ gtktypebuiltins_h, ] +
                            [ gtk_css_enum_h, gdkenum_h, gskenum_h, ],
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
gtkversion_cdata.set('GTK_VERSION', gtk_version)
gtkversion_cdata.set('GTK_API_VERSION', gtk_api_version)

if host_machine.system() == 'windows'
  gtkversion_cdata.set('EXE_MANIFEST_ARCHITECTURE', '*')
endif

gtkversion = configure_file(input: 'gtkversion.h.in',
                            output: 'gtkversion.h',
                            configuration: gtkversion_cdata,
                            install_dir: join_paths(gtk_includedir, 'gtk-4.0/gtk'))

gtk_gen_headers = [
  gtkmarshal_h,
  gtktypebuiltins_h,
  gtkprivatetypebuiltins_h,
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
  fribidi_dep,
  cairogobj_dep,
  cairo_dep,
  fontconfig_dep,
  pixbuf_dep,
  atk_dep,
  epoxy_dep,
  libm,
  graphene_dep,
]

if harfbuzz_dep.found() and pangoft_dep.found()
  gtk_deps += [ harfbuzz_dep, pangoft_dep ]
  gtk_sources += files([
  'language-names.c',
  'script-names.c',
  ])
endif

if x11_enabled
  x11_data_prefix = dependency('x11').get_pkgconfig_variable('prefix')

  gtk_cargs += [ '-DX11_DATA_PREFIX="@0@"'.format(x11_data_prefix), ]
  gtk_sources += gtk_x11_sources
  gtk_deps += [ xi_dep, x11_dep, ]
endif

if wayland_enabled
  gtk_sources += gtk_wayland_sources
endif

if broadway_enabled
  gtk_sources += ['gtkimcontextbroadway.c']
endif

if win32_enabled
  gtk_cargs += ['-DGTK_PRINT_BACKENDS="file,lpr"']
  gtk_deps += [ giowin32_dep, pangowin32_dep ]

  gtk_deps += [cc.find_library('advapi32'),
               cc.find_library('comctl32'),
               cc.find_library('dwmapi'),
               cc.find_library('imm32'),
               cc.find_library('setupapi'),
               cc.find_library('winmm')]

  gtkwin32rc = configure_file(input: 'gtk-win32.rc.body.in',
                              output: 'gtk-win32.rc.body',
                              configuration: gtkversion_cdata)

  win32_manifest = configure_file(input: 'libgtk4.manifest.in',
                                  output: 'libgtk4.manifest',
                                  configuration: gtkversion_cdata)

  win32res = import('windows').compile_resources(gtkwin32rc, include_directories : win32rcinc)
  gtk_sources += win32res
else
  gtk_deps += [ atkbridge_dep, ]
endif

# So we don't add these twice
if x11_enabled or wayland_enabled
  gtk_sources += gtk_use_wayland_or_x11_c_sources
  gtk_deps += pangoft_dep
endif

if cloudproviders_enabled
  gtk_deps += cloudproviders_dep
endif

# Unconditional. If libintl isn't found,
# the object just does nothing being in the deplist
gtk_deps += libintl_dep

gtk_settings_schemas = [
  'org.gtk.gtk4.Settings.FileChooser.gschema.xml',
  'org.gtk.gtk4.Settings.ColorChooser.gschema.xml',
  'org.gtk.gtk4.Settings.EmojiChooser.gschema.xml',
  'org.gtk.gtk4.Settings.Debug.gschema.xml',
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
    cdata.set('HAVE_' + nl_enum, cc.has_header_symbol('langinfo.h', nl_enum))
  endforeach
endif

# Library
libgtk = shared_library('gtk-4',
                        soversion: gtk_soversion,
                        sources: [typefuncs, gtk_sources, gtkmarshal_h, gtkprivatetypebuiltins_h],
                        c_args: gtk_cargs + common_cflags,
                        include_directories: [confinc, gdkinc, gskinc, gtkinc],
                        dependencies: gtk_deps + [libgtk_css_dep, libgdk_dep, libgsk_dep],
                        link_with: [libgtk_css, libgdk, libgsk, ],
                        link_args: common_ldflags,
                        install: true)

gtk_dep_sources = [gtkversion, gtktypebuiltins_h]
# Introspection
build_gir = get_option('introspection')
if build_gir
  gir_args = [
    '--quiet',
  ]

  # We may build some of the dependencies as sub-projects; this means
  # that we need to depend on the built introspection data, instead of
  # the installed one
  gdk_gir_inc = [ 'cairo-1.0', 'Gio-2.0', ]

  if pixbuf_dep.type_name() == 'internal'
    gdk_gir_inc += subproject('gdk-pixbuf').get_variable('gdkpixbuf_gir').get(0)
  else
    gdk_gir_inc += 'GdkPixbuf-2.0'
  endif

  if pango_dep.type_name() == 'internal'
    gdk_gir_inc += subproject('pango').get_variable('pango_gir').get(0)
  else
    gdk_gir_inc += 'Pango-1.0'
  endif

  gdk_gir = gnome.generate_gir(libgtk,
                               sources: gdk_public_headers + gdk_public_sources + [ gdkenum_h ],
                               namespace: 'Gdk',
                               nsversion: gtk_api_version,
                               identifier_prefix: 'Gdk',
                               symbol_prefix: 'gdk',
                               export_packages: 'gtk4',
                               includes: gdk_gir_inc,
                               header: 'gdk/gdk.h',
                               install: true,
                               extra_args: gir_args + [
                                 '-DGDK_COMPILATION',
                               ])
  gdk_gir_dep = declare_dependency(sources: gdk_gir)
  gtk_dep_sources += gdk_gir

  if x11_enabled
    gdk_x11_gir = gnome.generate_gir(libgtk,
                                     sources: gdk_x11_public_headers + gdk_x11_sources,
                                     namespace: 'GdkX11',
                                     nsversion: gtk_api_version,
                                     identifier_prefix: 'Gdk',
                                     symbol_prefix: 'gdk',
                                     export_packages: 'gtk4-x11',
                                     includes: [ gdk_gir[0], 'xlib-2.0', ],
                                     install: true,
                                     dependencies: gdk_gir_dep,
                                     header: 'gdk/gdkx.h',
                                     extra_args: gir_args + [
                                       '-DGDK_COMPILATION',
                                     ])
    gtk_dep_sources += gdk_x11_gir
  endif

  gsk_gir_inc = [ gdk_gir[0], ]
  if graphene_dep.type_name() == 'internal'
    gsk_gir_inc += subproject('graphene').get_variable('graphene_gir').get(0)
  else
    gsk_gir_inc += 'Graphene-1.0'
  endif

  gsk_gir = gnome.generate_gir(libgtk,
                               sources: gsk_public_headers + gsk_public_sources + [ gskenum_h ],
                               namespace: 'Gsk',
                               nsversion: gtk_api_version,
                               identifier_prefix: 'Gsk',
                               symbol_prefix: 'gsk',
                               export_packages: 'gtk4',
                               includes: gsk_gir_inc,
                               header: 'gsk/gsk.h',
                               install: true,
                               dependencies: [gdk_gir_dep, graphene_dep],
                               extra_args: gir_args + [
                                 '-DGSK_COMPILATION',
                               ])
  gsk_gir_dep = declare_dependency(dependencies: [gdk_gir_dep, graphene_dep],
                                   sources: gsk_gir)
  gtk_dep_sources += gsk_gir

  gtk_introspection_sources = [
    gtk_public_headers,
    gtk_public_sources,
    gtk_deprecated_headers,
    gtk_deprecated_sources,
    a11y_headers,
    a11y_sources,
    gtktypebuiltins_h,
    gtkversion,
  ]

  if os_unix
    gtk_introspection_sources += gtk_unix_print_sources
  endif

  if os_win32
    gtk_introspection_sources += gtk_win32_print_sources
  endif

  gtk_gir = gnome.generate_gir(libgtk,
                               sources: gtk_introspection_sources,
                               namespace: 'Gtk',
                               nsversion: gtk_api_version,
                               identifier_prefix: 'Gtk',
                               symbol_prefix: 'gtk',
                               export_packages: 'gtk4',
                               includes: [ gdk_gir[0], gsk_gir[0], 'Atk-1.0', ],
                               header: 'gtk/gtk.h',
                               install: true,
                               dependencies: gsk_gir_dep,
                               extra_args: gir_args + [
                                 '-DGTK_COMPILATION',
                                 '--c-include=gtk/gtk-a11y.h',
                               ])
  gtk_dep_sources += gtk_gir
endif

libgtk_dep = declare_dependency(sources: gtk_dep_sources,
                                include_directories: [confinc, gtkinc],
                                dependencies: gtk_deps + [libgtk_css_dep, libgdk_dep, libgsk_dep],
                                link_with: libgtk,
                                link_args: common_ldflags)

subdir('tools')
