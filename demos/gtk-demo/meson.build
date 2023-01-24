# These should be in the order you want them to appear in the
# demo app, which means alphabetized by demo title, not filename
demos = files([
  'application_demo.c',
  'assistant.c',
  'builder.c',
  'clipboard.c',
  'combobox.c',
  'constraints.c',
  'constraints_interactive.c',
  'constraints_vfl.c',
  'constraints_builder.c',
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
  'entry_completion.c',
  'entry_undo.c',
  'errorstates.c',
  'expander.c',
  'filtermodel.c',
  'fishbowl.c',
  'fixed.c',
  'fontrendering.c',
  'frames.c',
  'gears.c',
  'gestures.c',
  'glarea.c',
  'gltransition.c',
  'headerbar.c',
  'hypertext.c',
  'iconscroll.c',
  'iconview.c',
  'iconview_edit.c',
  'images.c',
  'infobar.c',
  'layoutmanager.c',
  'layoutmanager2.c',
  'links.c',
  'listbox.c',
  'listbox_controls.c',
  'menu.c',
  'flowbox.c',
  'list_store.c',
  'listview_applauncher.c',
  'listview_clocks.c',
  'listview_colors.c',
  'listview_filebrowser.c',
  'listview_minesweeper.c',
  'dropdown.c',
  'listview_settings.c',
  'listview_ucd.c',
  'listview_weather.c',
  'listview_words.c',
  'markup.c',
  'overlay.c',
  'overlay_decorative.c',
  'paint.c',
  'paintable.c',
  'paintable_animated.c',
  'paintable_emblem.c',
  'paintable_mediastream.c',
  'paintable_symbolic.c',
  'panes.c',
  'password_entry.c',
  'peg_solitaire.c',
  'pickers.c',
  'printing.c',
  'read_more.c',
  'revealer.c',
  'rotated_text.c',
  'scale.c',
  'search_entry.c',
  'search_entry2.c',
  'shadertoy.c',
  'shortcuts.c',
  'shortcut_triggers.c',
  'sidebar.c',
  'sizegroup.c',
  'sliding_puzzle.c',
  'spinbutton.c',
  'spinner.c',
  'stack.c',
  'tabs.c',
  'tagged_entry.c',
  'textmask.c',
  'textundo.c',
  'textview.c',
  'textscroll.c',
  'themes.c',
  'theming_style_classes.c',
  'transparent.c',
  'tree_store.c',
  'video_player.c',
  'font_features.c',
  'paintable_glyph.c',
])

gtkdemo_deps = [ libgtk_dep, hbgobj_dep, hbcairo_dep ]

extra_demo_sources = files([
  'main.c',
  'fontify.c',
  'gtkfishbowl.c',
  'fontplane.c',
  'gtkgears.c',
  'gtkshaderbin.c',
  'gtkshadertoy.c',
  'gtkshaderstack.c',
  'gskshaderpaintable.c',
  'puzzlepiece.c',
  'bluroverlay.c',
  'demoimage.c',
  'demotaggedentry.c',
  'demochild.c',
  'demolayout.c',
  'demowidget.c',
  'demo2layout.c',
  'singular_value_decomposition.c',
  'four_point_transform.c',
  'demo2widget.c',
  'demo3widget.c',
  'pixbufpaintable.c',
  'script-names.c',
  'unicode-names.c',
  'suggestionentry.c',
  'language-names.c',
  'fontvariations.c',
  'rangeedit.c',
  'fontcolors.c',
  'glyphpicker.c',
  'colorpicker.c',
  'glyphmodel.c',
  'fontpicker.c',
])

if os_unix
  demos += files('pagesetup.c')
endif

librsvg_dep = dependency('librsvg-2.0', version: '>= 2.52.0', required: false)

if librsvg_dep.found()
  demos += files('paintable_svg.c')
  extra_demo_sources += files(['svgpaintable.c'])
  gtkdemo_deps += [ librsvg_dep ]
endif

gtkdemo_args = [ '-DGDK_DISABLE_DEPRECATED', '-DGTK_DISABLE_DEPRECATED', ]

demos_h = custom_target('gtk4 demo header',
  output: 'demos.h',
  input: demos,
  command: [ find_program('geninclude.py'), '@OUTPUT@', '@INPUT@' ],
)

objcopy_supports_add_symbol = false
objcopy = find_program('objcopy', required : false)
if objcopy.found()
  objcopy_supports_add_symbol = run_command(objcopy, '--help', check: false).stdout().contains('--add-symbol')
endif

ld = find_program('ld', required : false)

if not meson.is_cross_build() and build_machine.cpu_family() != 'arm' and build_machine.system() == 'linux' and objcopy.found() and objcopy_supports_add_symbol and ld.found()
  glib_compile_resources = find_program('glib-compile-resources')

  # Create the resource blob
  gtkdemo_gresource = custom_target('gtkdemo.gresource',
      input : 'demo.gresource.xml',
      output : 'gtkdemo.gresource',
      depfile : 'gtkdemo.gresource.d',
      command : [glib_compile_resources,
                 '--generate',
                 '--internal',
                 '--target=@OUTPUT@',
                 '--dependency-file=@DEPFILE@',
                 '--sourcedir=' + meson.current_source_dir(),
                 '--sourcedir=' + meson.current_build_dir(),
                 '@INPUT@'])

  # Create resource data file
  gtkdemo_resources_c = custom_target('gtkdemo_resources.c',
      input : 'demo.gresource.xml',
      output : 'gtkdemo_resources.c',
      depfile : 'gtkdemo_resources.c.d',
      command : [glib_compile_resources,
                 '--generate-source',
                 '--internal',
                 '--target=@OUTPUT@',
                 '--dependency-file=@DEPFILE@',
                 '--sourcedir=' + meson.current_source_dir(),
                 '--sourcedir=' + meson.current_build_dir(),
                 '--external-data',
                 '--c-name', '_g_binary_gtkdemo',
                 '@INPUT@'])

  # Create object file containing resource data
  gtkdemo_resources_binary = custom_target('gtkdemo_resources.o',
      input : gtkdemo_gresource,
      output : 'gtkdemo_resources.o',
      command : [ld,
                 '-z', 'noexecstack',
                 '-r',
                 '-b','binary',
                 '@INPUT@',
                 '-o','@OUTPUT@'])

  # Rename symbol to match the one in the C file
  gtkdemo_resources_o = custom_target('gtkdemo_resources2.o',
    input : gtkdemo_resources_binary,
    output : 'gtkdemo_resources2.o',
    command : [objcopy,
                 '--strip-all',
                 '--add-symbol','_g_binary_gtkdemo_resource_data=.data:0',
                 '@INPUT@',
                 '@OUTPUT@'])

  gtkdemo_resources = [
      gtkdemo_resources_c,
      gtkdemo_resources_o,
    ]
else
  gtkdemo_resources = gnome.compile_resources('gtkdemo_resources',
    'demo.gresource.xml',
    source_dir: '.',
  )
endif

# Use a subset of compiler flags
demo_cflags = []
foreach flag: common_cflags
  if flag not in ['-Werror=missing-prototypes', '-Wmissing-prototypes',
                  '-Werror=missing-declarations', '-Wmissing-declarations',
                  '-fvisibility=hidden']
    demo_cflags += flag
  endif
endforeach

gtkdemo_deps += [ demo_conf_h ]

executable('gtk4-demo',
  sources: [demos, demos_h, extra_demo_sources, gtkdemo_resources],
  c_args: gtkdemo_args + demo_cflags,
  dependencies: gtkdemo_deps,
  include_directories: confinc,
  win_subsystem: 'windows',
  link_args: extra_demo_ldflags,
  install: true,
)

executable('gtk4-demo-application',
  sources: ['application.c', gtkdemo_resources],
  c_args: gtkdemo_args + common_cflags,
  dependencies: gtkdemo_deps,
  include_directories: confinc,
  win_subsystem: 'windows',
  link_args: extra_demo_ldflags,
  install: true,
)

# icons
icontheme_dir = join_paths(gtk_datadir, 'icons/hicolor')

foreach size: ['scalable', 'symbolic']
  install_subdir('data/' + size, install_dir: icontheme_dir)
endforeach

# desktop file
install_data('org.gtk.Demo4.desktop', install_dir: gtk_applicationsdir)

# GSettings
install_data('org.gtk.Demo4.gschema.xml', install_dir: gtk_schemasdir)
gnome.compile_schemas()

# appdata
configure_file(
  input: 'org.gtk.Demo4.appdata.xml.in',
  output: 'org.gtk.Demo4.appdata.xml',
  configuration: appdata_config,
  install_dir: gtk_appdatadir
)
