atk_public_sources = files([
  'atkaction.c',
  'atkcomponent.c',
  'atkdocument.c',
  'atkeditabletext.c',
  'atkgobjectaccessible.c',
  'atkhyperlink.c',
  'atkhyperlinkimpl.c',
  'atkhypertext.c',
  'atkimage.c',
  'atknoopobject.c',
  'atknoopobjectfactory.c',
  'atkobject.c',
  'atkobjectfactory.c',
  'atkplug.c',
  'atkprivate.c',
  'atkrange.c',
  'atkregistry.c',
  'atkrelation.c',
  'atkrelationset.c',
  'atkselection.c',
  'atksocket.c',
  'atkstate.c',
  'atkstateset.c',
  'atkstreamablecontent.c',
  'atktable.c',
  'atktablecell.c',
  'atktext.c',
  'atkutil.c',
  'atkvalue.c',
  'atkwindow.c',
])

atk_public_headers = files([
  'atkaction.h',
  'atkcomponent.h',
  'atkdocument.h',
  'atkeditabletext.h',
  'atkenums.h',
  'atkgobjectaccessible.h',
  'atkhyperlink.h',
  'atkhyperlinkimpl.h',
  'atkhypertext.h',
  'atkimage.h',
  'atknoopobject.h',
  'atknoopobjectfactory.h',
  'atkobject.h',
  'atkobjectfactory.h',
  'atkplug.h',
  'atkrange.h',
  'atkregistry.h',
  'atkrelation.h',
  'atkrelationset.h',
  'atkselection.h',
  'atksocket.h',
  'atkstate.h',
  'atkstateset.h',
  'atkstreamablecontent.h',
  'atktable.h',
  'atktablecell.h',
  'atktext.h',
  'atktypes.h',
  'atkutil.h',
  'atkvalue.h',
  'atkwindow.h',
])

install_headers(atk_public_headers + ['atk.h'], subdir: 'gtk-4.0/atk')

# Marshallers
atk_marshals = gnome.genmarshal('atkmarshal',
  sources: 'atkmarshal.list',
  prefix: 'atk_marshal',
)
atk_marshal_h = atk_marshals[1]

# Enumerations for GType
atk_enums = gnome.mkenums('atkenumtypes',
  sources: atk_public_headers,
  c_template: 'atkenumtypes.c.template',
  h_template: 'atkenumtypes.h.template',
  install_dir: gtk_includedir / 'gtk-4.0' / 'atk',
  install_header: true,
)
atk_enum_h = atk_enums[1]

atk_deps = [
  libgdk_dep,
]

libatk = static_library('atk',
  sources: [atk_public_sources, atk_enums, atk_marshals],
  dependencies: atk_deps,
  include_directories: [confinc, ],
  c_args: [
    '-DGTK_COMPILATION',
    '-DG_LOG_DOMAIN="Atk"',
    '-DG_LOG_USE_STRUCTURED=1',
    '-DGLIB_DISABLE_DEPRECATION_WARNINGS',
    '-DATK_DISABLE_DEPRECATION_WARNINGS',
    '-DATK_LOCALEDIR="@0@"'.format(join_paths(gtk_datadir, 'locale')),
  ],
  link_with: libgdk,
  link_args: common_ldflags,
)

# We don't have link_with: to internal static libs here on purpose, just
# list the dependencies and generated headers and such, for use in the
# "public" libgtk_dep used by internal executables.
libatk_dep = declare_dependency(
  include_directories: confinc,
  sources: [atk_enum_h],
  dependencies: libgdk_dep,
)
