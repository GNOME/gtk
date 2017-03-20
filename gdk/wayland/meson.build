gdk_wayland_sources = files([
  'gdkapplaunchcontext-wayland.c',
  'gdkcursor-wayland.c',
  'gdkdevice-wayland.c',
  'gdkdisplay-wayland.c',
  'gdkdnd-wayland.c',
  'gdkeventsource.c',
  'gdkglcontext-wayland.c',
  'gdkkeys-wayland.c',
  'gdkmonitor-wayland.c',
  'gdkscreen-wayland.c',
  'gdkselection-wayland.c',
  'gdkvulkancontext-wayland.c',
  'gdkwindow-wayland.c',
  'wm-button-layout-translation.c',
])

gdk_wayland_public_headers = files([
  'gdkwaylanddevice.h',
  'gdkwaylanddisplay.h',
  'gdkwaylandglcontext.h',
  'gdkwaylandmonitor.h',
  'gdkwaylandselection.h',
  'gdkwaylandwindow.h'
])

install_headers(gdk_wayland_public_headers, subdir: 'gtk-4.0/gdk/wayland/')
install_headers('gdkwayland.h', subdir: 'gtk-4.0/gdk/')

gdk_wayland_deps = [
  shmlib,
  xkbdep,
  wlclientdep,
  wlprotocolsdep,
  wlcursordep,
  wlegldep,
]

# wayland protocols

proto_dir = dependency('wayland-protocols').get_pkgconfig_variable('pkgdatadir')
assert(proto_dir != '', 'Could not get pkgdatadir from wayland-protocols.pc')

wayland_scanner = find_program('wayland-scanner')
genprotocols = find_program('genprotocolfiles.py')

proto_sources = [
  ['gtk-shell',
   'protocol/gtk-shell.xml'],

  ['gtk-primary-selection',
   'protocol/gtk-primary-selection.xml'],

  ['pointer-gestures-unstable-v1',
   join_paths(proto_dir, 'unstable/pointer-gestures/pointer-gestures-unstable-v1.xml')],

  ['xdg-shell-unstable-v6',
   join_paths(proto_dir, 'unstable/xdg-shell/xdg-shell-unstable-v6.xml')],

  ['xdg-foreign-unstable-v1',
   join_paths(proto_dir, 'unstable/xdg-foreign/xdg-foreign-unstable-v1.xml')],

  ['tablet-unstable-v2',
   join_paths(proto_dir, 'unstable/tablet/tablet-unstable-v2.xml')],
]

gdk_wayland_gen_headers = []

# FIXME: there's some protostability/protoname stuff in Makefile.am I don't grok
foreach p : proto_sources
  output_base = p[0]
  input = p[1]

  gdk_wayland_gen_headers += custom_target('@0@ client header'.format(output_base),
    input : input,
    output : '@0@-client-protocol.h'.format(output_base),
    command: [genprotocols, wayland_scanner, '@INPUT@', '@OUTPUT@', 'client-header'])

  gdk_wayland_sources += custom_target('@0@ source'.format(output_base),
    input : input,
    output : '@0@-protocol.c'.format(output_base),
    command: [genprotocols, wayland_scanner, '@INPUT@', '@OUTPUT@', 'code'])
endforeach

libgdk_wayland = static_library('libgdk-wayland',
  gdk_wayland_sources, gdk_wayland_gen_headers, gdkconfig, gdkenum_h,
  include_directories: [confinc, gdkinc],
  c_args: ['-DGDK_COMPILATION', '-DG_LOG_DOMAIN="Gdk"'],
  dependencies: [gdk_deps, gdk_wayland_deps])
