
## These should be in the order you want them to appear in the
## demo app, which means alphabetized by demo title, not filename
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
  'offscreen_window.c',
  'offscreen_window2.c',
  'overlay.c',
  'overlay2.c',
  'paint.c',
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


gtkdemo_deps = [libgtk_dep]

if pango_dep.version().version_compare('>=1.44.0') and harfbuzz_dep.found() and cc.has_header_symbol(
  'hb-ot.h',
  'hb_ot_var_get_axis_count',
  dependencies: harfbuzz_dep
)
  demos += files('font_features.c')
elif harfbuzz_dep.found() and pangoft_dep.found()
  demos += files('font_features.c')
  gtkdemo_deps += [harfbuzz_dep, pangoft_dep]
endif

if os_unix
  demos += files('pagesetup.c')
endif

gtkdemo_sources = demos + files([
  'gtkfishbowl.c',
  'gtkfishbowl.h',
  'main.c',
])


geninclude = find_program('geninclude.py')
demos_h = custom_target(
  'gtk3 demo header',
  output  : 'demos.h',
  input   : demos,
  command : [geninclude, '@OUTPUT@', '@INPUT@'],
)

gtkdemo_resources = gnome.compile_resources(
  'gtkdemo_resources',
  'demo.gresource.xml',
  source_dir: '.'
)

gtkdemo = executable(
  'gtk3-demo',
  gtkdemo_sources,
  demos_h,
  gtkdemo_resources,
  dependencies: gtkdemo_deps,
  include_directories : confinc,
  install: true
)

gtkapplicationdemo = executable(
  'gtk3-demo-application',
  'application.c',
  demos_h,
  gtkdemo_resources,
  dependencies: gtkdemo_deps,
  include_directories : confinc,
  install: true
)

gtk_settings_schemas = [
  'org.gtk.Settings.FileChooser.gschema.xml',
  'org.gtk.Settings.ColorChooser.gschema.xml',
  'org.gtk.Settings.EmojiChooser.gschema.xml',
  'org.gtk.Settings.Debug.gschema.xml',
]

foreach s: [ '16', '22', '24', '32', '48', '256']
  icon_destdir = join_paths(gtk_datadir, 'icons', 'hicolor', '@0@x@0@'.format(s), 'apps')
  icons = [join_paths('data', '@0@x@0@'.format(s), 'gtk3-demo.png'),
           join_paths('data', '@0@x@0@'.format(s), 'gtk3-demo-symbolic.symbolic.png')]
  install_data(icons, install_dir: icon_destdir)
endforeach

# desktop file
install_data('gtk3-demo.desktop', install_dir: gtk_applicationsdir)

# GSettings
install_data('org.gtk.Demo.gschema.xml', install_dir: gtk_schemasdir)
gnome.compile_schemas(depend_files: files(['org.gtk.Demo.gschema.xml']))
