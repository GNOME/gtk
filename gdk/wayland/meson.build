

if wayland_enabled
  #TODO: This should come from pkg-config --variable=pkgdatadir wayland-protocols
  wayland_protocols_datadir = '/home/baedert/Source/gnome-configure/share/wayland-protocols/'
  wayland_scanner = find_program('wayland-scanner')
  genprotocols = find_program('genprotocolfiles.py')

  gtk_shell_h = custom_target(
    'gtk shell protocol header',
    input  : 'protocol/gtk-shell.xml',
    output : 'gtk-shell-client-protocol.h',
    command: [genprotocols, wayland_scanner, '@INPUT@', '@OUTPUT@', 'client-header']
  )

  gtk_shell_c = custom_target(
    'gtk shell protocol source',
    input  : 'protocol/gtk-shell.xml',
    output : 'gtk-shell-protocol.c',
    command: [genprotocols, wayland_scanner, '@INPUT@', '@OUTPUT@', 'code']
  )

  pointer_gestures_h = custom_target(
    'pointer gestures header',
    input  : wayland_protocols_datadir + 'unstable/pointer-gestures/pointer-gestures-unstable-v1.xml',
    output : 'pointer-gestures-unstable-v1-client-protocol.h',
    command: [genprotocols, wayland_scanner, '@INPUT@', '@OUTPUT@', 'client-header']
  )

  pointer_gestures_c = custom_target(
    'pointer gestures source',
    input  : wayland_protocols_datadir + 'unstable/pointer-gestures/pointer-gestures-unstable-v1.xml',
    output : 'pointer-gestures-unstable-v1-protocol.c',
    command: [genprotocols, wayland_scanner, '@INPUT@', '@OUTPUT@', 'code']
  )

  primary_selection_h = custom_target(
    'primary selection header',
    input  : 'protocol/gtk-primary-selection.xml',
    output : 'gtk-primary-selection-client-protocol.h',
    command: [genprotocols, wayland_scanner,'@INPUT@', '@OUTPUT@', 'client-header']
  )

  primary_selection_c = custom_target(
    'primary selection source',
    input  : 'protocol/gtk-primary-selection.xml',
    output : 'gtk-primary-selection-protocol.c',
    command: [genprotocols, wayland_scanner,'@INPUT@', '@OUTPUT@', 'code']
  )

  xdg_shell_h = custom_target(
    'xdg shell v6 header',
    input  : wayland_protocols_datadir + 'unstable/xdg-shell/xdg-shell-unstable-v6.xml',
    output : 'xdg-shell-unstable-v6-client-protocol.h',
    command: [genprotocols, wayland_scanner,'@INPUT@', '@OUTPUT@', 'client-header']
  )

  xdg_shell_c = custom_target(
    'xdg shell v6 source',
    input  : wayland_protocols_datadir + 'unstable/xdg-shell/xdg-shell-unstable-v6.xml',
    output : 'xdg-shell-unstable-v6-protocol.c',
    command: [genprotocols, wayland_scanner,'@INPUT@', '@OUTPUT@', 'code']
  )

  xdg_foreign_h = custom_target(
    'xdg foreign v1 header',
    input  : wayland_protocols_datadir + 'unstable/xdg-foreign/xdg-foreign-unstable-v1.xml',
    output : 'xdg-foreign-unstable-v1-client-protocol.h',
    command: [genprotocols, wayland_scanner,'@INPUT@', '@OUTPUT@', 'client-header']
  )

  xdg_foreign_c = custom_target(
    'xdg foreign v1 source',
    input  : wayland_protocols_datadir + 'unstable/xdg-foreign/xdg-foreign-unstable-v1.xml',
    output : 'xdg-foreign-unstable-v1-protocol.c',
    command: [genprotocols, wayland_scanner,'@INPUT@', '@OUTPUT@', 'code']
  )

  tablet_h = custom_target(
    'tablet v2 header',
    input  : wayland_protocols_datadir + 'unstable/tablet/tablet-unstable-v2.xml',
    output : 'tablet-unstable-v2-client-protocol.h',
    command: [genprotocols, wayland_scanner,'@INPUT@', '@OUTPUT@', 'client-header']
  )

  tablet_c = custom_target(
    'tablet v2 source',
    input  : wayland_protocols_datadir + 'unstable/tablet/tablet-unstable-v2.xml',
    output : 'tablet-unstable-v2-protocol.c',
    command: [genprotocols, wayland_scanner,'@INPUT@', '@OUTPUT@', 'code']
  )



  gdk_sources += [
    gtk_shell_c,
    gtk_shell_h,
    pointer_gestures_h,
    pointer_gestures_c,
    primary_selection_h,
    primary_selection_c,
    xdg_shell_h,
    xdg_shell_c,
    xdg_foreign_h,
    xdg_foreign_c,
    tablet_h,
    tablet_c
  ]

endif
