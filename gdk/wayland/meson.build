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
  'gdkwaylandcolor.c',
  'gdkwaylandpresentationtime.c',
  'wm-button-layout-translation.c',
])

gdk_wayland_public_headers = files([
  'gdkwaylanddevice.h',
  'gdkwaylanddisplay.h',
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

# Fields:
#  - name: protocol name
#  - stability: protocol stability ('private', 'stable' or 'unstable')
#  - version: protocol version
#  - required: wayland_protocols version check
proto_sources = [
  {
    'name': 'gtk-shell',
    'stability': 'private',
  },
  {
    'name': 'primary-selection',
    'stability': 'unstable',
    'version': 1,
  },
  {
    'name': 'pointer-gestures',
    'stability': 'unstable',
    'version': 1,
  },
  {
    'name': 'viewporter',
    'stability': 'stable',
  },
  {
    'name': 'xdg-shell',
    'stability': 'unstable',
    'version': 6,
  },
  {
    'name': 'xdg-shell',
    'stability': 'stable',
  },
  {
    'name': 'xdg-foreign',
    'stability': 'unstable',
    'version': 1,
  },
  {
    'name': 'xdg-foreign',
    'stability': 'unstable',
    'version': 2,
  },
  {
    'name': 'tablet',
    'stability': 'unstable',
    'version': 2,
  },
  {
    'name': 'keyboard-shortcuts-inhibit',
    'stability': 'unstable',
    'version': 1,
  },
  {
    'name': 'server-decoration',
    'stability': 'private',
  },
  {
    'name': 'xdg-output',
    'stability': 'unstable',
    'version': 1,
  },
  {
    'name': 'idle-inhibit',
    'stability': 'unstable',
    'version': 1,
  },
  {
    'name': 'xdg-activation',
    'stability': 'staging',
    'version': 1,
  },
  {
    'name': 'fractional-scale',
    'stability': 'staging',
    'version': 1,
  },
  {
    'name': 'linux-dmabuf',
    'stability': 'unstable',
    'version': 1,
  },
  {
    'name': 'presentation-time',
    'stability': 'stable',
    'version': 1,
  },
  {
    'name': 'single-pixel-buffer',
    'stability': 'staging',
    'version': 1,
  },
  {
    'name': 'xdg-dialog',
    'stability': 'staging',
    'version': 1,
  },
  {
    'name': 'xx-color-management',
    'stability': 'private',
    'version': 4,
  },
  {
    'name': 'xdg-system-bell',
    'stability': 'private',
    'version': 1,
  },
]

gdk_wayland_gen_headers = []

wlmod = import('unstable-wayland')
foreach p: proto_sources
  if wlprotocolsdep.version().version_compare(p.get('required', '>=0'))
    if p.get('stability') == 'private'
      if (p.has_key('version'))
        proto_file = 'protocol/@0@-v@1@.xml'.format(p.get('name'), p.get('version'))
      else
        proto_file = 'protocol/@0@.xml'.format(p.get('name'))
      endif
    elif p.get('stability') == 'stable'
      proto_file = wlmod.find_protocol(p.get('name'),
        state: p.get('stability'),
      )
    else
      proto_file = wlmod.find_protocol(p.get('name'),
        state: p.get('stability'),
        version: p.get('version'),
      )
    endif

    # Returns a list [.c, .h]
    gen = wlmod.scan_xml(proto_file)
    assert(gen.length() == 2)
    gdk_wayland_sources += gen[0]
    gdk_wayland_gen_headers += gen[1]
  endif
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
