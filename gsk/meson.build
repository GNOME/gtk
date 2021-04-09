gsk_private_gl_shaders = [
  'gl/resources/preamble.glsl',
  'gl/resources/preamble.fs.glsl',
  'gl/resources/preamble.vs.glsl',
  'gl/resources/border.glsl',
  'gl/resources/blit.glsl',
  'gl/resources/coloring.glsl',
  'gl/resources/color.glsl',
  'gl/resources/linear_gradient.glsl',
  'gl/resources/radial_gradient.glsl',
  'gl/resources/conic_gradient.glsl',
  'gl/resources/color_matrix.glsl',
  'gl/resources/blur.glsl',
  'gl/resources/inset_shadow.glsl',
  'gl/resources/outset_shadow.glsl',
  'gl/resources/unblurred_outset_shadow.glsl',
  'gl/resources/cross_fade.glsl',
  'gl/resources/blend.glsl',
  'gl/resources/repeat.glsl',
  'gl/resources/custom.glsl',
]

gsk_private_ngl_shaders = [
  'ngl/resources/preamble.glsl',
  'ngl/resources/preamble.fs.glsl',
  'ngl/resources/preamble.vs.glsl',
  'ngl/resources/border.glsl',
  'ngl/resources/blit.glsl',
  'ngl/resources/coloring.glsl',
  'ngl/resources/color.glsl',
  'ngl/resources/linear_gradient.glsl',
  'ngl/resources/radial_gradient.glsl',
  'ngl/resources/conic_gradient.glsl',
  'ngl/resources/color_matrix.glsl',
  'ngl/resources/blur.glsl',
  'ngl/resources/inset_shadow.glsl',
  'ngl/resources/outset_shadow.glsl',
  'ngl/resources/unblurred_outset_shadow.glsl',
  'ngl/resources/cross_fade.glsl',
  'ngl/resources/blend.glsl',
  'ngl/resources/repeat.glsl',
  'ngl/resources/custom.glsl',
  'ngl/resources/filled_border.glsl',
]

gsk_public_sources = files([
  'gskdiff.c',
  'gskcairorenderer.c',
  'gskglshader.c',
  'gskrenderer.c',
  'gskrendernode.c',
  'gskrendernodeimpl.c',
  'gskrendernodeparser.c',
  'gskroundedrect.c',
  'gsktransform.c',
  'gl/gskglrenderer.c',
  'ngl/gsknglrenderer.c',
])

gsk_private_sources = files([
  'gskcairoblur.c',
  'gskdebug.c',
  'gskprivate.c',
  'gskprofiler.c',
  'gl/gskglshaderbuilder.c',
  'gl/gskglprofiler.c',
  'gl/gskglglyphcache.c',
  'gl/gskgldriver.c',
  'gl/gskglrenderops.c',
  'gl/gskglshadowcache.c',
  'gl/gskgltextureatlas.c',
  'gl/gskgliconcache.c',
  'gl/opbuffer.c',
  'gl/stb_rect_pack.c',
  'ngl/gsknglattachmentstate.c',
  'ngl/gsknglbuffer.c',
  'ngl/gsknglcommandqueue.c',
  'ngl/gsknglcompiler.c',
  'ngl/gskngldriver.c',
  'ngl/gsknglglyphlibrary.c',
  'ngl/gskngliconlibrary.c',
  'ngl/gsknglprogram.c',
  'ngl/gsknglrenderjob.c',
  'ngl/gsknglshadowlibrary.c',
  'ngl/gskngltexturelibrary.c',
  'ngl/gskngluniformstate.c',
  'ngl/gskngltexturepool.c',
  'ngl/fp16.c',
])

gsk_public_headers = files([
  'gskcairorenderer.h',
  'gskenums.h',
  'gskglshader.h',
  'gskrenderer.h',
  'gskrendernode.h',
  'gskroundedrect.h',
  'gsktransform.h',
  'gsktypes.h',
  'gsk-autocleanup.h',
])
install_headers(gsk_public_headers, 'gsk.h', subdir: 'gtk-4.0/gsk')

gsk_public_gl_headers = files([
  'gl/gskglrenderer.h',
  'ngl/gsknglrenderer.h',
])
install_headers(gsk_public_gl_headers, subdir: 'gtk-4.0/gsk/gl')
gsk_public_headers += gsk_public_gl_headers

if get_variable('broadway_enabled')
  gsk_public_broadway_headers = files([
    'broadway/gskbroadwayrenderer.h'
  ])
  install_headers(gsk_public_broadway_headers, subdir: 'gtk-4.0/gsk/broadway')
  gsk_public_headers += gsk_public_broadway_headers
endif

if have_vulkan
  gsk_public_vulkan_headers = files([
    'vulkan/gskvulkanrenderer.h'
  ])
  install_headers(gsk_public_vulkan_headers, subdir: 'gtk-4.0/gsk/vulkan')
  gsk_public_headers += gsk_public_vulkan_headers
endif

gsk_private_vulkan_shaders = []
# This is an odd split because we use configure_file() below to workaround
# a limitation in meson preventing using custom_target() with gnome.compile_resources()
# and that requires file paths, but we also need to have dependencies during development
# on constantly regenerated files.
gsk_private_vulkan_compiled_shaders = []
gsk_private_vulkan_compiled_shaders_deps = []

if have_vulkan
  gsk_private_sources += files([
    'vulkan/gskvulkanblendmodepipeline.c',
    'vulkan/gskvulkanblurpipeline.c',
    'vulkan/gskvulkanborderpipeline.c',
    'vulkan/gskvulkanboxshadowpipeline.c',
    'vulkan/gskvulkanbuffer.c',
    'vulkan/gskvulkanclip.c',
    'vulkan/gskvulkancolorpipeline.c',
    'vulkan/gskvulkancolortextpipeline.c',
    'vulkan/gskvulkancrossfadepipeline.c',
    'vulkan/gskvulkancommandpool.c',
    'vulkan/gskvulkaneffectpipeline.c',
    'vulkan/gskvulkanglyphcache.c',
    'vulkan/gskvulkanlineargradientpipeline.c',
    'vulkan/gskvulkanimage.c',
    'vulkan/gskvulkantextpipeline.c',
    'vulkan/gskvulkantexturepipeline.c',
    'vulkan/gskvulkanmemory.c',
    'vulkan/gskvulkanpipeline.c',
    'vulkan/gskvulkanpushconstants.c',
    'vulkan/gskvulkanrender.c',
    'vulkan/gskvulkanrenderer.c',
    'vulkan/gskvulkanrenderpass.c',
    'vulkan/gskvulkanshader.c',
  ])

  subdir('vulkan/resources')
endif # have_vulkan

if get_variable('broadway_enabled')
  gsk_public_sources += files([
    'broadway/gskbroadwayrenderer.c',
  ])
endif

gsk_resources_xml = configure_file(output: 'gsk.resources.xml',
  input: 'gen-gsk-gresources-xml.py',
  command: [
    find_program('gen-gsk-gresources-xml.py'),
    '@OUTPUT@',
    gsk_private_gl_shaders,
    gsk_private_ngl_shaders,
    gsk_private_vulkan_compiled_shaders,
    gsk_private_vulkan_shaders
  ],
)

gsk_enums = gnome.mkenums('gskenumtypes',
  sources: gsk_public_headers,
  c_template: 'gskenumtypes.c.template',
  h_template: 'gskenumtypes.h.template',
  install_dir: gtk_includedir / 'gtk-4.0/gsk',
  install_header: true,
)

gskenum_h = gsk_enums[1]

gskresources = gnome.compile_resources('gskresources',
  gsk_resources_xml,
  dependencies: gsk_private_vulkan_compiled_shaders_deps,
  source_dir: '.',
  c_name: '_gsk',
  extra_args: [ '--manual-register', ],
)

gsk_gen_headers = [ gskenum_h, ]

gsk_deps = [
  graphene_dep,
  pango_dep,
  cairo_dep,
  cairo_csi_dep,
  pixbuf_dep,
  libgdk_dep,
]

libgsk = static_library('gsk',
  sources: [
    gsk_public_sources,
    gsk_private_sources,
    gsk_enums,
    gskresources,
  ],
  dependencies: gsk_deps,
  include_directories: [ confinc, ],
  c_args: [
    '-DGTK_COMPILATION',
    '-DG_LOG_DOMAIN="Gsk"',
    '-DG_LOG_STRUCTURED=1',
  ] + common_cflags,
  link_with: libgdk,
)

# We don't have link_with: to internal static libs here on purpose, just
# list the dependencies and generated headers and such, for use in the
# "public" libgtk_dep used by internal executables.
libgsk_dep = declare_dependency(include_directories: [ confinc, ],
  sources: [ gskenum_h, ],
  dependencies: [libgdk_dep, cairo_csi_dep],
)
