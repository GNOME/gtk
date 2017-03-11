gsk_public_sources = files([
  'gskrenderer.c',
  'gskrendernode.c',
  'gskrendernodeimpl.c',
  'gskroundedrect.c',
  'gsktexture.c',
])

# FIXME: + $(gsk_private_vulkan_source_c)
gsk_private_sources = files([
  'gskcairoblur.c',
  'gskcairorenderer.c',
  'gskdebug.c',
  'gskgldriver.c',
  'gskglprofiler.c',
  'gskglrenderer.c',
  'gskprivate.c',
  'gskprofiler.c',
  'gskshaderbuilder.c',
])

# FIXME: do we need this variable?
gsk_sources = gsk_public_sources + gsk_private_sources

gsk_headers = files([
  'gskenums.h',
  'gskrenderer.h',
  'gskrendernode.h',
  'gsktypes.h',
  'gskcairorendererprivate.h',
  'gskdebugprivate.h',
  'gskgldriverprivate.h',
  'gskglprofilerprivate.h',
  'gskglrendererprivate.h',
  'gskprivate.h',
  'gskprofilerprivate.h',
  'gskrendererprivate.h',
  'gskrendernodeprivate.h',
  'gskshaderbuilderprivate.h',
])

gskenum_h = custom_target(
  'gskenum_h',
  output  : 'gskenumtypes.h',
  input   : gsk_headers,
  command : [mkenum, perl, glib_mkenums, meson.current_source_dir() + '/gskenumtypes.h.template', '@OUTPUT@', '@INPUT@'])

gskenum_c = custom_target(
  'gskenum_c',
  output  : 'gskenumtypes.c',
  input   : gsk_headers,
  depends : gskenum_h,
  command : [mkenum, perl, glib_mkenums, meson.current_source_dir() + '/gskenumtypes.c.template', '@OUTPUT@', '@INPUT@'])

gskresources = gnome.compile_resources(
  'gskresources',
  'gsk.resources.xml',
  source_dir: '.',
  c_name: '_gsk',
  extra_args: '--manual-register'
)


gsk_deps = [
  graphene_dep,
  pango_dep,
  cairo_dep,
  pixbuf_dep,
  libgdk_dep,
]

libgsk = static_library(
  'gsk',
  sources: [gsk_sources, gskenum_h, gskresources],
  dependencies: gsk_deps,
  include_directories: [confinc],
  c_args: ['-DGSK_COMPILATION'],
)

libgsk_dep = declare_dependency(
  depends: gsk_sources,
  link_with: libgsk,
  link_args: ['-Bsymbolic'],
  include_directories: [confinc],
  sources: [gskenum_h, gskresources]
)
