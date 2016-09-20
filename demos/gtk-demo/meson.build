
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
  'textmask.c',
  'textview.c',
  'textscroll.c',
  'theming_style_classes.c',
  'toolpalette.c',
  'transparent.c',
  'tree_store.c',
])


gtkdemo_deps = [libgtk_dep]

if build_font_demo
  demos += files('font_features.c')
  gtkdemo_deps += harfbuzz_dep
endif

if os_unix
  demos += files('pagesetup.c')
endif

gtkdemo_sources = demos + files([
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
  gui_app: true
)
