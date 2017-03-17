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

# FIXME: install gsk_public_headers ?
gsk_public_headers = files([
  'gskenums.h',
  'gskrenderer.h',
  'gskrendernode.h',
  'gskroundedrect.h',
  'gsktexture.h',
  'gsktypes.h',
])

if have_vulkan
  gsk_private_sources += files([
    'gskvulkanblendpipeline.c',
    'gskvulkanborderpipeline.c',
    'gskvulkanboxshadowpipeline.c',
    'gskvulkanbuffer.c',
    'gskvulkanclip.c',
    'gskvulkancolorpipeline.c',
    'gskvulkancommandpool.c',
    'gskvulkaneffectpipeline.c',
    'gskvulkanlineargradientpipeline.c',
    'gskvulkanimage.c',
    'gskvulkanmemory.c',
    'gskvulkanpipeline.c',
    'gskvulkanpushconstants.c',
    'gskvulkanrender.c',
    'gskvulkanrenderer.c',
    'gskvulkanrenderpass.c',
    'gskvulkanshader.c',
  ])

# FIXME: vulkan shaders
#gsk_private_vulkan_include_shaders =',
#  'resources/vulkan/clip.frag.glsl',
#  'resources/vulkan/clip.vert.glsl',
#  'resources/vulkan/constants.glsl',
#  'resources/vulkan/rounded-rect.glsl
#gsk_private_vulkan_fragment_shaders =',
#  'resources/vulkan/blend.frag',
#  'resources/vulkan/border.frag',
#  'resources/vulkan/color.frag',
#  'resources/vulkan/color-matrix.frag',
#  'resources/vulkan/inset-shadow.frag',
#  'resources/vulkan/linear.frag',
#  'resources/vulkan/outset-shadow.frag
#gsk_private_vulkan_vertex_shaders =',
#  'resources/vulkan/blend.vert',
#  'resources/vulkan/border.vert',
#  'resources/vulkan/color.vert',
#  'resources/vulkan/color-matrix.vert',
#  'resources/vulkan/inset-shadow.vert',
#  'resources/vulkan/linear.vert',
#  'resources/vulkan/outset-shadow.vert
#gsk_private_vulkan_shaders =',
#  '$(gsk_private_vulkan_fragment_shaders)',
#  '$(gsk_private_vulkan_vertex_shaders)
#gsk_private_vulkan_compiled_shaders =',
#  '$(gsk_private_vulkan_fragment_shaders:.frag=.frag.spv)',
#  '$(gsk_private_vulkan_vertex_shaders:.vert=.vert.spv)',
#  '$(gsk_private_vulkan_fragment_shaders:.frag=-clip.frag.spv)',
#  '$(gsk_private_vulkan_vertex_shaders:.vert=-clip.vert.spv)',
#  '$(gsk_private_vulkan_fragment_shaders:.frag=-clip-rounded.frag.spv)',
#  '$(gsk_private_vulkan_vertex_shaders:.vert=-clip-rounded.vert.spv)
endif

# FIXME: do we need this variable?
gsk_sources = gsk_public_sources + gsk_private_sources

gskenum_h = custom_target(
  'gskenum_h',
  output  : 'gskenumtypes.h',
  input   : gsk_public_headers,
  command : [mkenum, perl, glib_mkenums, meson.current_source_dir() + '/gskenumtypes.h.template', '@OUTPUT@', '@INPUT@'])

gskenum_c = custom_target(
  'gskenum_c',
  output  : 'gskenumtypes.c',
  input   : gsk_public_headers,
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
  c_args: ['-DGSK_COMPILATION', '-DG_LOG_DOMAIN="Gsk"'],
)

libgsk_dep = declare_dependency(
  depends: gsk_sources,
  link_with: libgsk,
  link_args: ['-Bsymbolic'],
  include_directories: [confinc],
  sources: [gskenum_h, gskresources]
)
