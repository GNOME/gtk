subdir('cursor')

gdk_wayland_sources = files([
  'gdkapplaunchcontext-wayland.c',
  'gdkcairocontext-wayland.c',
  'gdkclipboard-wayland.c',
  'gdkcursor-wayland.c',
  'gdkdevice-wayland.c',
  'gdkdisplay-wayland.c',
  'gdkdrag-wayland.c',
  'gdkdrop-wayland.c',
  'gdkeventsource.c',
  'gdkglcontext-wayland.c',
  'gdkkeys-wayland.c',
  'gdkmonitor-wayland.c',
  'gdkprimary-wayland.c',
  'gdkvulkancontext-wayland.c',
  'gdksurface-wayland.c',
  'wm-button-layout-translation.c',
])

gdk_wayland_public_headers = files([
  'gdkwaylanddevice.h',
  'gdkwaylanddisplay.h',
  'gdkwaylandglcontext.h',
  'gdkwaylandmonitor.h',
  'gdkwaylandseat.h',
  'gdkwaylandsurface.h',
])

install_headers(gdk_wayland_public_headers, 'gdkwayland.h', subdir: 'gtk-4.0/gdk/wayland/')

gdk_wayland_deps = [
  shmlib,
  xkbdep,
  wlclientdep,
  wlprotocolsdep,
  wlegldep,
]

wayland_scanner = find_program('wayland-scanner')

# Format:
#  - protocol name
#  - protocol stability ('private', 'stable' or 'unstable')
#  - protocol version (if stability is 'unstable')
proto_sources = [
  ['gtk-shell', 'private', ],
  ['primary-selection', 'unstable', 'v1', ],
  ['pointer-gestures', 'unstable', 'v1', ],
  ['xdg-shell', 'unstable', 'v6', ],
  ['xdg-shell', 'stable', ],
  ['xdg-foreign', 'unstable', 'v1', ],
  ['tablet', 'unstable', 'v2', ],
  ['keyboard-shortcuts-inhibit', 'unstable', 'v1', ],
  ['server-decoration', 'private' ],
  ['xdg-output', 'unstable', 'v1', ],
  ['idle-inhibit', 'unstable', 'v1', ],
  ['xdg-activation', 'staging', 'v1', ],
]

gdk_wayland_gen_headers = []

foreach p: proto_sources
  proto_name = p.get(0)
  proto_stability = p.get(1)

  if proto_stability == 'stable'
    output_base = proto_name
    input = files(join_paths(wlproto_dir, '@0@/@1@/@2@.xml'.format(proto_stability, proto_name, output_base)))
  elif proto_stability == 'staging'
    proto_version = p.get(2)
    output_base = '@0@-@1@'.format(proto_name, proto_version)
    input = join_paths(wlproto_dir, '@0@/@1@/@2@.xml'.format(proto_stability, proto_name, output_base))
  elif proto_stability == 'private'
    output_base = proto_name
    input = files('protocol/@0@.xml'.format(proto_name))
  else
    proto_version = p.get(2)
    output_base = '@0@-@1@-@2@'.format(proto_name, proto_stability, proto_version)
    input = files(join_paths(wlproto_dir, '@0@/@1@/@2@.xml'.format(proto_stability, proto_name, output_base)))
  endif

  gdk_wayland_gen_headers += custom_target('@0@ client header'.format(output_base),
    input: input,
    output: '@0@-client-protocol.h'.format(output_base),
    command: [
      wayland_scanner,
      'client-header',
      '@INPUT@', '@OUTPUT@',
    ],
  )

  gdk_wayland_sources += custom_target('@0@ source'.format(output_base),
    input: input,
    output: '@0@-protocol.c'.format(output_base),
    command: [
      wayland_scanner,
      'private-code',
      '@INPUT@', '@OUTPUT@',
    ],
  )
endforeach

libgdk_wayland = static_library('gdk-wayland',
  sources: [
    gdk_wayland_sources,
    gdk_wayland_gen_headers,
    gdkconfig,
    gdkenum_h,
  ],
  include_directories: [ confinc, gdkinc, ],
  c_args: [
    '-DGTK_COMPILATION',
    '-DG_LOG_DOMAIN="Gdk"',
  ] + common_cflags,
  link_with: [ libwayland_cursor, ],
  dependencies: [ gdk_deps, gdk_wayland_deps ],
)
