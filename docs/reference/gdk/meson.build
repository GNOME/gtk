private_headers = [
  'gdkintl.h',
  'gdkmarshalers.h',
  'gdkkeysyms.h',
  'gdkinternals.h',
  'gdk-private.h',
  'gdkapplaunchcontextprivate.h',
  'gdkclipboardprivate.h',
  'gdkcontentformatsprivate.h',
  'gdkcontentproviderprivate.h',
  'gdkcursorprivate.h',
  'gdkdeviceprivate.h',
  'gdkdevicepadprivate.h',
  'gdkdevicetoolprivate.h',
  'gdkdisplaymanagerprivate.h',
  'gdkdisplayprivate.h',
  'gdkdndprivate.h',
  'gdkdragprivate.h',
  'gdkdrawcontextprivate.h',
  'gdkdropprivate.h',
  'gdkeventsprivate.h',
  'gdkframeclockidleprivate.h',
  'gdkframeclockprivate.h',
  'gdkglcontextprivate.h',
  'gdkgltextureprivate.h',
  'gdkkeysprivate.h',
  'gdkmonitorprivate.h',
  'gdkmemorytextureprivate.h',
  'gdkpipeiostreamprivate.h',
  'gdkscreenprivate.h',
  'gdkseatdefaultprivate.h',
  'gdkseatprivate.h',
  'gdksnapshotprivate.h',
  'gdksurfaceimpl.h',
  'gdktextureprivate.h',
  'gdkvulkancontextprivate.h',
  'keyname-table.h',
  'gdkprivate-x11.h',
  'x11/gdkeventsource.h',
  'gtk-primary-selection-client-protocol.h',
  'gtk-shell-client-protocol.h',
  'keyboard-shortcuts-inhibit-unstable-v1-client-protocol.h',
  'pointer-gestures-unstable-v1-client-protocol.h',
  'server-decoration-client-protocol.h',
  'tablet-unstable-v2-client-protocol.h',
  'xdg-foreign-unstable-v1-client-protocol.h',
  'xdg-shell-unstable-v6-client-protocol.h',
  'win32',
  'quartz',
  'broadway',
  'mir'
]

images = [
  'images/rotated-text.png',
  'images/default_cursor.png',
  'images/help_cursor.png',
  'images/pointer_cursor.png',
  'images/context_menu_cursor.png',
  'images/progress_cursor.png',
  'images/wait_cursor.png',
  'images/cell_cursor.png',
  'images/crosshair_cursor.png',
  'images/text_cursor.png',
  'images/vertical_text_cursor.png',
  'images/alias_cursor.png',
  'images/copy_cursor.png',
  'images/no_drop_cursor.png',
  'images/move_cursor.png',
  'images/not_allowed_cursor.png',
  'images/grab_cursor.png',
  'images/grabbing_cursor.png',
  'images/all_scroll_cursor.png',
  'images/col_resize_cursor.png',
  'images/row_resize_cursor.png',
  'images/n_resize_cursor.png',
  'images/e_resize_cursor.png',
  'images/s_resize_cursor.png',
  'images/w_resize_cursor.png',
  'images/ne_resize_cursor.png',
  'images/nw_resize_cursor.png',
  'images/sw_resize_cursor.png',
  'images/se_resize_cursor.png',
  'images/ew_resize_cursor.png',
  'images/ns_resize_cursor.png',
  'images/nesw_resize_cursor.png',
  'images/nwse_resize_cursor.png',
  'images/zoom_in_cursor.png',
  'images/zoom_out_cursor.png',
]

src_dir = [ gdkinc ]

if x11_enabled
  src_dir += [ gdkx11_inc ]
endif

if wayland_enabled
  src_dir += [ gdkwayland_inc ]
endif

if get_option('gtk_doc')
  configure_file(input: 'version.xml.in', output: 'version.xml', configuration: version_conf)

  gnome.gtkdoc('gdk4',
               mode: 'none',
               main_xml: 'gdk4-docs.xml',
               src_dir: src_dir,
               dependencies: libgtk_dep,
               gobject_typesfile: join_paths(meson.current_source_dir(), 'gdk4.types'),
               scan_args: [
                 '--ignore-decorators=_GDK_EXTERN|G_GNUC_WARN_UNUSED_RESULT',
                 '--ignore-headers=' + ' '.join(private_headers),
               ],
               fixxref_args: [
                 '--html-dir=@0@'.format(docpath),
                 '--extra-dir=@0@'.format(join_paths(glib_docpath, 'glib')),
                 '--extra-dir=@0@'.format(join_paths(glib_docpath, 'gobject')),
                 '--extra-dir=@0@'.format(join_paths(glib_docpath, 'gio')),
                 '--extra-dir=@0@'.format(cairo_docpath),
               ],
               html_assets: images,
               install: true)
endif
