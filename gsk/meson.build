gsk_private_source_shaders = [
  'resources/glsl/blend.fs.glsl',
  'resources/glsl/blend.vs.glsl',
  'resources/glsl/blit.fs.glsl',
  'resources/glsl/blit.vs.glsl',
  'resources/glsl/es2_common.fs.glsl',
  'resources/glsl/es2_common.vs.glsl',
  'resources/glsl/gl3_common.fs.glsl',
  'resources/glsl/gl3_common.vs.glsl',
  'resources/glsl/gl_common.fs.glsl',
  'resources/glsl/gl_common.vs.glsl',
]

gsk_public_sources = files([
  'gskrenderer.c',
  'gskrendernode.c',
  'gskrendernodeimpl.c',
  'gskroundedrect.c',
  'gsktexture.c',
])

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

gsk_public_headers = files([
  'gskenums.h',
  'gskrenderer.h',
  'gskrendernode.h',
  'gskroundedrect.h',
  'gsktexture.h',
  'gsktypes.h',
])

install_headers(gsk_public_headers, 'gsk.h', subdir : 'gtk-4.0/gsk')

gsk_private_vulkan_shaders = []
gsk_private_vulkan_compiled_shaders = []

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

# FIXME: what's up with these?
#  gsk_private_vulkan_include_shaders = [
#    'resources/vulkan/clip.frag.glsl',
#    'resources/vulkan/clip.vert.glsl',
#    'resources/vulkan/constants.glsl',
#    'resources/vulkan/rounded-rect.glsl',
#  ]

  gsk_private_vulkan_fragment_shaders = [
    'resources/vulkan/blend.frag',
    'resources/vulkan/border.frag',
    'resources/vulkan/color.frag',
    'resources/vulkan/color-matrix.frag',
    'resources/vulkan/inset-shadow.frag',
    'resources/vulkan/linear.frag',
    'resources/vulkan/outset-shadow.frag',
  ]

  gsk_private_vulkan_vertex_shaders = [
    'resources/vulkan/blend.vert',
    'resources/vulkan/border.vert',
    'resources/vulkan/color.vert',
    'resources/vulkan/color-matrix.vert',
    'resources/vulkan/inset-shadow.vert',
    'resources/vulkan/linear.vert',
    'resources/vulkan/outset-shadow.vert',
  ]

  gsk_private_vulkan_shaders += gsk_private_vulkan_fragment_shaders
  gsk_private_vulkan_shaders += gsk_private_vulkan_vertex_shaders

  foreach shader : gsk_private_vulkan_shaders
    basefn = shader.split('.').get(0)
    suffix = shader.split('.').get(1)

    # FIXME: the compiled shaders seem to exist in the srcdir already, do we
    # want to or need to add a target to create them with glslc? Do we need to
    # check for glslc at all then?
    #
    # stage_arg = suffix == 'frag' ? '-fshader-stage=fragment' : '-fshader-stage=vertex'
    # compiled_shader = custom_target('xyz', input : shader,
    #   output : '@0@.@1@.spv'.format(basefn, suffix),
    #   command: [glslc, stage_arg, '-DCLIP_NONE', '-o', '@OUTPUT@'])

    gsk_private_vulkan_compiled_shaders += '@0@.@1@.spv'.format(basefn, suffix)
    gsk_private_vulkan_compiled_shaders += '@0@-clip.@1@.spv'.format(basefn, suffix)
    gsk_private_vulkan_compiled_shaders += '@0@-clip-rounded.@1@.spv'.format(basefn, suffix)
  endforeach

endif # have_vulkan

gsk_resources_xml = configure_file(output : 'gsk.resources.xml',
  input : 'gen-gsk-resources-xml.py',
  command : [find_program('gen-gsk-resources-xml.py'),
             '@OUTPUT@',
             gsk_private_source_shaders,
             gsk_private_vulkan_compiled_shaders,
             gsk_private_vulkan_shaders])

# FIXME: do we need this variable?
gsk_sources = gsk_public_sources + gsk_private_sources

gsk_enums = gnome.mkenums('gskenumtypes',
  sources: gsk_public_headers,
  c_template : 'gskenumtypes.c.template',
  h_template : 'gskenumtypes.h.template',
  install_dir : 'include/gtk-4.0/gsk',
  install_header : true)

gskenum_h = gsk_enums[1]

gskresources = gnome.compile_resources('gskresources',
  gsk_resources_xml,
  source_dir: '.',
  c_name: '_gsk',
  extra_args: '--manual-register')

gsk_gen_headers = [gskenum_h]

gsk_deps = [
  graphene_dep,
  pango_dep,
  cairo_dep,
  pixbuf_dep,
  libgdk_dep,
]

libgsk = static_library('gsk',
  sources: [gsk_sources, gsk_enums, gskresources],
  dependencies: gsk_deps,
  include_directories: [confinc],
  c_args: ['-DGSK_COMPILATION', '-DG_LOG_DOMAIN="Gsk"'] + common_cflags,
  link_with: libgdk,
  link_args: common_ldflags)

# We don't have link_with: to internal static libs here on purpose, just
# list the dependencies and generated headers and such, for use in the
# "public" libgtk_dep used by internal executables.
libgsk_dep = declare_dependency(
  depends: gsk_sources,
  include_directories: [confinc],
  sources: [gskenum_h, gskresources],
  dependencies: libgdk_dep)
