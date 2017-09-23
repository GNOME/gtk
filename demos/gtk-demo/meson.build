# These should be in the order you want them to appear in the
# demo app, which means alphabetized by demo title, not filename
demos = files([
  'application_demo.c',
  'assistant.c',
  'builder.c',
  'button_box.c',
  'changedisplay.c',
  'clipboard.c',
  'colorsel.c',
  'combobox.c',
  'css_accordion.c',
  'css_basics.c',
  'css_blendmodes.c',
  'css_multiplebgs.c',
  'css_pixbufs.c',
  'css_shadows.c',
  'cursors.c',
  'dialog.c',
  'drawingarea.c',
  'editable_cells.c',
  'entry_buffer.c',
  'entry_completion.c',
  'event_axes.c',
  'expander.c',
  'filtermodel.c',
  'fishbowl.c',
  'widgetbowl.c',
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
  'panes.c',
  'pickers.c',
  'pixbufs.c',
  'popover.c',
  'printing.c',
  'revealer.c',
  'rotated_text.c',
  'scale.c',
  'search_entry.c',
  'search_entry2.c',
  'shadertoy.c',
  'shortcuts.c',
  'sidebar.c',
  'sizegroup.c',
  'spinbutton.c',
  'spinner.c',
  'stack.c',
  'tabs.c',
  'textmask.c',
  'textview.c',
  'textscroll.c',
  'theming_style_classes.c',
  'toolpalette.c',
  'transparent.c',
  'tree_store.c',
])

gtkdemo_deps = [ libgtk_dep, ]

if harfbuzz_dep.found() and pangoft_dep.found()
  demos += files('font_features.c')
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
           'main.c', 'gtkfishbowl.c', demos, demos_h, gtkdemo_resources,
           c_args: gtkdemo_args,
           dependencies: gtkdemo_deps,
           include_directories: confinc,
           gui_app: true,
           install: true)

executable('gtk4-demo-application',
           'application.c', gtkdemo_resources,
           c_args: gtkdemo_args,
           dependencies: gtkdemo_deps,
           include_directories: confinc,
           gui_app: true,
           install: true)

# icons
icontheme_dir = join_paths(gtk_datadir, 'icons/hicolor')

foreach icon_size: [ '16x16', '22x22', '24x24', '32x32', '48x48', '256x256', ]
  gtk_demo_icons_dir = join_paths(icontheme_dir, '@0@/apps'.format(icon_size))
  install_data('data/@0@/gtk4-demo.png'.format(icon_size),
               install_dir: gtk_demo_icons_dir)
  install_data('data/@0@/gtk4-demo-symbolic.symbolic.png'.format(icon_size),
               install_dir: gtk_demo_icons_dir)
endforeach

# desktop file
install_data('gtk4-demo.desktop', install_dir: gtk_applicationsdir)

# GSettings
install_data('org.gtk.Demo.gschema.xml', install_dir: gtk_schemasdir)
gnome.compile_schemas()
