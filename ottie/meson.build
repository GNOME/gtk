ottie_public_sources = files([
  'ottiecreation.c',
  'ottiepaintable.c',
  'ottieplayer.c',
])

ottie_private_sources = files([
  'ottiecolorvalue.c',
  'ottiecomposition.c',
  'ottiecompositionlayer.c',
  'ottiedoublevalue.c',
  'ottieellipseshape.c',
  'ottiefillshape.c',
  'ottiegroupshape.c',
  'ottielayer.c',
  'ottienulllayer.c',
  'ottieobject.c',
  'ottieparser.c',
  'ottiepathshape.c',
  'ottiepathvalue.c',
  'ottiepointvalue.c',
  'ottiepoint3dvalue.c',
  'ottierectshape.c',
  'ottierender.c',
  'ottieshape.c',
  'ottieshapelayer.c',
  'ottiestrokeshape.c',
  'ottietransform.c',
  'ottietrimshape.c',
])

ottie_public_headers = files([
  'ottie.h',
  'ottiecreation.h',
  'ottiepaintable.h',
  'ottieplayer.h',
])

install_headers(ottie_public_headers, 'ottie.h', subdir: 'gtk-4.0/ottie')

json_glib_dep = dependency('json-glib-1.0', required: true)
ottie_deps = [
  libm,
  glib_dep,
  gobject_dep,
  platform_gio_dep,
  libgdk_dep,
  libgsk_dep,
  json_glib_dep
]

libottie = static_library('ottie',
                            sources: [
                              ottie_public_sources,
                              ottie_private_sources,
                            ],
                            dependencies: ottie_deps,
                            include_directories: [ confinc, ],
                            c_args: [
                              '-DGTK_COMPILATION',
                              '-DG_LOG_DOMAIN="Ottie"',
                            ] + common_cflags,
                            link_with: [libgdk, libgsk ],
                            link_args: common_ldflags)

# We don't have link_with: to internal static libs here on purpose, just
# list the dependencies and generated headers and such, for use in the
# "public" libgtk_dep used by internal executables.
libottie_dep = declare_dependency(include_directories: [ confinc, ],
                                  dependencies: ottie_deps)

