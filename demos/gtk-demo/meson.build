# These should be in the order you want them to appear in the
# demo app, which means alphabetized by demo title, not filename
demos = files([
  'application_demo.c',
  'assistant.c',
  'builder.c',
  'changedisplay.c',
  'clipboard.c',
  'colorsel.c',
  'combobox.c',
  'constraints.c',
  'constraints2.c',
  'constraints3.c',
  'css_accordion.c',
  'css_basics.c',
  'css_blendmodes.c',
  'css_multiplebgs.c',
  'css_pixbufs.c',
  'css_shadows.c',
  'cursors.c',
  'dialog.c',
  'drawingarea.c',
  'dnd.c',
  'editable_cells.c',
  'entry_buffer.c',
  'entry_completion.c',
  'expander.c',
  'filtermodel.c',
  'fishbowl.c',
  'fixed.c',
  'fontrendering.c',
  'foreigndrawing.c',
  'gestures.c',
  'glarea.c',
  'headerbar.c',
  'hypertext.c',
  'iconview.c',
  'iconview_edit.c',
  'images.c',
  'infobar.c',
  'links.c',
  'listbox.c',
  'flowbox.c',
  'list_store.c',
  'markup.c',
  'menus.c',
  'modelbutton.c',
  'overlay.c',
  'overlay2.c',
  'paint.c',
  'paintable.c',
  'paintable_animated.c',
  'paintable_mediastream.c',
  'panes.c',
  'password_entry.c',
  'pickers.c',
  'pixbufs.c',
  'popover.c',
  'printing.c',
  'revealer.c',
  'rotated_text.c',
  'scale.c',
  'search_entry.c',
  'search_entry2.c',
  'shortcuts.c',
  'sidebar.c',
  'sizegroup.c',
  'sliding_puzzle.c',
  'spinbutton.c',
  'spinner.c',
  'stack.c',
  'tabs.c',
  'tagged_entry.c',
  'textmask.c',
  'textview.c',
  'textscroll.c',
  'themes.c',
  'theming_style_classes.c',
  'transparent.c',
  'tree_store.c',
  'video_player.c',
])

gtkdemo_deps = [ libgtk_dep, ]

extra_demo_sources = files(['main.c',
                            'gtkfishbowl.c',
                            'fontplane.c',
                            'gtkgears.c',
                            'puzzlepiece.c',
                            'bluroverlay.c',
                            'demotaggedentry.c'])

if harfbuzz_dep.found() and pangoft_dep.found()
  demos += files('font_features.c')
  extra_demo_sources += files(['script-names.c', 'language-names.c'])
  gtkdemo_deps += [ harfbuzz_dep, ]
endif

if os_unix
  demos += files('pagesetup.c')
endif

gtkdemo_args = [ '-DGDK_DISABLE_DEPRECATED', '-DGTK_DISABLE_DEPRECATED', ]

demos_h = custom_target('gtk4 demo header',
                        output: 'demos.h',
                        input: demos,
                        command: [ find_program('geninclude.py'), '@OUTPUT@', '@INPUT@' ])

gtkdemo_resources = gnome.compile_resources('gtkdemo_resources',
                                            'demo.gresource.xml',
                                            source_dir: '.')

executable('gtk4-demo',
           demos, demos_h, extra_demo_sources, gtkdemo_resources,
           c_args: gtkdemo_args,
           dependencies: gtkdemo_deps,
           include_directories: confinc,
           gui_app: true,
           link_args: extra_demo_ldflags,
           install: true)

executable('gtk4-demo-application',
           'application.c', gtkdemo_resources,
           c_args: gtkdemo_args,
           dependencies: gtkdemo_deps,
           include_directories: confinc,
           gui_app: true,
           link_args: extra_demo_ldflags,
           install: true)

# icons
icontheme_dir = join_paths(gtk_datadir, 'icons/hicolor')

foreach size: ['scalable', 'symbolic']
  install_subdir('data/' + size,
    install_dir: icontheme_dir
  )
endforeach

# desktop file
install_data('org.gtk.Demo4.desktop', install_dir: gtk_applicationsdir)

# GSettings
install_data('org.gtk.Demo4.gschema.xml', install_dir: gtk_schemasdir)
gnome.compile_schemas()

# appdata
install_data('org.gtk.Demo4.appdata.xml', install_dir: gtk_appdatadir)
