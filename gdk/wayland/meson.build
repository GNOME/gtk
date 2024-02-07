subdir('cursor')

gdk_wayland_sources = files([
  'gdkapplaunchcontext-wayland.c',
  'gdkcairocontext-wayland.c',
  'gdkclipboard-wayland.c',
  'gdkcursor-wayland.c',
  'gdkdevice-wayland.c',
  'gdkdevicepad-wayland.c',
  'gdkdisplay-wayland.c',
  'gdkdmabuf-wayland.c',
  'gdkdrag-wayland.c',
  'gdkdragsurface-wayland.c',
  'gdkdrop-wayland.c',
  'gdkeventsource.c',
  'gdkglcontext-wayland.c',
  'gdkkeymap-wayland.c',
  'gdkmonitor-wayland.c',
  'gdkprimary-wayland.c',
  'gdkseat-wayland.c',
  'gdksurface-wayland.c',
  'gdksubsurface-wayland.c',
  'gdktoplevel-wayland.c',
  'gdkpopup-wayland.c',
  'gdkvulkancontext-wayland.c',
  'gdkwaylandpresentationtime.c',
  'wm-button-layout-translation.c',
])

gdk_wayland_public_headers = files([
  'gdkwaylanddevice.h',
  'gdkwaylanddisplay.h',
  'gdkwaylanddmabufformats.h',
  'gdkwaylandglcontext.h',
  'gdkwaylandmonitor.h',
  'gdkwaylandpopup.h',
  'gdkwaylandseat.h',
  'gdkwaylandsurface.h',
  'gdkwaylandtoplevel.h',
])

install_headers(gdk_wayland_public_headers, 'gdkwayland.h', subdir: 'gtk-4.0/gdk/wayland/')

gdk_wayland_deps = [
  shmlib,
  xkbdep,
  wlclientdep,
  wlprotocolsdep,
  wlegldep,
]

wlmod = import('unstable-wayland')
proto_sources = [
  'protocol/gtk-shell.xml',
  'protocol/server-decoration.xml',
  wlmod.find_protocol('primary-selection', state: 'unstable', version: 1),
  wlmod.find_protocol('pointer-gestures', state: 'unstable', version: 1),
  wlmod.find_protocol('viewporter', state: 'stable'),
  wlmod.find_protocol('xdg-shell', state: 'unstable', version: 6),
  wlmod.find_protocol('xdg-shell', state: 'stable'),
  wlmod.find_protocol('xdg-foreign', state: 'unstable', version: 1),
  wlmod.find_protocol('xdg-foreign', state: 'unstable', version: 2),
  wlmod.find_protocol('tablet', state: 'unstable', version: 2),
  wlmod.find_protocol('keyboard-shortcuts-inhibit', state: 'unstable', version: 1),
  wlmod.find_protocol('xdg-output', state: 'unstable', version: 1),
  wlmod.find_protocol('idle-inhibit', state: 'unstable', version: 1),
  wlmod.find_protocol('xdg-activation', state: 'staging', version: 1),
  wlmod.find_protocol('fractional-scale', state: 'staging', version: 1),
  wlmod.find_protocol('linux-dmabuf', state: 'unstable', version: 1),
  wlmod.find_protocol('presentation-time', state: 'stable'),
  wlmod.find_protocol('single-pixel-buffer', state: 'staging', version: 1),
]

gdk_wayland_gen_headers = []

foreach p: proto_sources
  # Returns a list [.c, .h]
  gen = wlmod.scan_xml(p)
  assert(gen.length() == 2)
  gdk_wayland_sources += gen[0]
  gdk_wayland_gen_headers += gen[1]
endforeach

libgdk_wayland = static_library('gdk-wayland',
  sources: [ gdk_wayland_sources, gdk_wayland_gen_headers, gdk_gen_headers ],
  include_directories: [ confinc, gdkinc, ],
  c_args: [
    '-DGTK_COMPILATION',
    '-DG_LOG_DOMAIN="Gdk"',
  ] + common_cflags,
  link_with: [ libwayland_cursor, ],
  dependencies: [ gdk_deps, gdk_wayland_deps ],
)

# Used to generate pkg-config Requires
wayland_public_deps = [wlclientdep]
