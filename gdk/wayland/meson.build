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

wayland_proto_targets = []
foreach p : proto_sources
  output_base = p[0]
  input = p[1]

  hdr = custom_target('@0@ client header'.format(output_base),
    input : input,
    output : '@0@-client-protocol.h'.format(output_base),
    command: [genprotocols, wayland_scanner, '@INPUT@', '@OUTPUT@', 'client-header'])

  src = custom_target('@0@ source'.format(output_base),
    input : input,
    output : '@0@-protocol.c'.format(output_base),
    command: [genprotocols, wayland_scanner, '@INPUT@', '@OUTPUT@', 'code'])

  wayland_proto_targets += [hdr, src]
endforeach

gdk_sources += wayland_proto_targets
