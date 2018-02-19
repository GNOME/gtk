# Note: this file is included from the top-level before gtk/meson.build.
# The actual input modules are then built in gtk/meson.build based on the
# defs we provide here. It has to be that way because included input methods
# need to be built before libgtk-4.so is built, so before gtk/meson.build, but
# all input methods also rely on gtk generated headers to be created first, so
# there is a bit of an ordering problem which we solve by collecting all the
# information here but moving the actual build definitions to gtk/meson.build.
build_dynamic_modules = false
dynamic_modules = get_option('dynamic-modules')
if dynamic_modules
  gmodule_supported = dependency('gmodule-no-export-2.0').get_pkgconfig_variable('gmodule_supported')
  if gmodule_supported == 'true'
    build_dynamic_modules = true
  else
    message('Modules are not supported according to gmodule-no-export-2.0.pc')
  endif
endif

all_immodules = backend_immodules

# Allow building some or all immodules included
included_immodules = get_option('included-immodules').split(',')
if included_immodules.contains('none')
  included_immodules = []
elif included_immodules.contains('all')
  included_immodules = all_immodules
endif

have_included_immodules = included_immodules.length() > 0

foreach im: included_immodules
  if not all_immodules.contains(im)
    error('The specified input method "@0@" is not available (available methods: @1@)'.format(im, ', '.join(all_immodules)))
  endif
endforeach

immodules_subdir = 'gtk-4.0/@0@/immodules'.format(gtk_binary_version)
immodules_install_dir = join_paths(gtk_libdir, immodules_subdir)

# Format:
#  - protocol name
#  - protocol stability ('stable' or 'unstable')
#  - protocol version (if stability is 'unstable')
proto_sources = [
  ['gtk-text-input', 'stable', ],
]

im_wayland_gen_headers = []
im_wayland_sources = files('imwayland.c')
wayland_scanner = find_program('wayland-scanner')
genprotocols = find_program('../../gdk/wayland/genprotocolfiles.py')

foreach p: proto_sources
  proto_name = p.get(0)
  proto_stability = p.get(1)

  if proto_stability == 'stable'
    output_base = proto_name
    input = '@0@.xml'.format(proto_name)
  else
    proto_version = p.get(2)
    output_base = '@0@-@1@-@2@'.format(proto_name, proto_stability, proto_version)
    input = join_paths(proto_dir, '@0@/@1@/@2@.xml'.format(proto_stability, proto_name, output_base))
  endif

  im_wayland_sources += custom_target('@0@ client header'.format(output_base),
                                      input: input,
                                      output: '@0@-client-protocol.h'.format(output_base),
                                      command: [
                                        genprotocols,
                                        wayland_scanner,
                                        '@INPUT@', '@OUTPUT@',
                                        'client-header',
                                      ])

  im_wayland_sources += custom_target('@0@ source'.format(output_base),
                                      input: input,
                                      output: '@0@-protocol.c'.format(output_base),
                                      command: [
                                        genprotocols,
                                        wayland_scanner,
                                        '@INPUT@', '@OUTPUT@',
                                        'code',
                                      ])
endforeach

method_defs = [
  ['broadway', files('imbroadway.c')],
  ['quartz', ('imquartz.c'), [], ('-xobjective-c')],
  ['xim', files('gtkimcontextxim.c')],
  ['ime', files('gtkimcontextime.c'), ['imm32']],
  ['wayland', im_wayland_sources],
]

inc_im_method_defs = []
dyn_im_method_defs = []

foreach m: method_defs
  im = m.get(0)
  srcs = m.get(1)
  cargs = m.get(3, [])
  libs = []

  # only use backend-specific input methods for backends that are enabled
  if all_immodules.contains(im)
    # check for extra libs lazily
    foreach libname: m.get(2, [])
      libs += [cc.find_library(libname)]
    endforeach

    if included_immodules.contains(im)
      cdata.set('INCLUDE_IM_@0@'.format(im.underscorify()), true)
      inc_im_method_defs += [[im, srcs, cargs, libs]]
    elif build_dynamic_modules
      dyn_im_method_defs += [[im, srcs, cargs, libs]]
    endif
  endif
endforeach
