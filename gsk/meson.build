gsk_private_gl_shaders = [
  'resources/glsl/blit.fs.glsl',
  'resources/glsl/blit.vs.glsl',
  'resources/glsl/color.fs.glsl',
  'resources/glsl/coloring.fs.glsl',
  'resources/glsl/color_matrix.fs.glsl',
  'resources/glsl/linear_gradient.fs.glsl',
  'resources/glsl/blur.fs.glsl',
  'resources/glsl/inset_shadow.fs.glsl',
  'resources/glsl/outset_shadow.fs.glsl',
  'resources/glsl/unblurred_outset_shadow.fs.glsl',
  'resources/glsl/border.fs.glsl',
  'resources/glsl/cross_fade.fs.glsl',
  'resources/glsl/blend.fs.glsl',
  'resources/glsl/es2_common.fs.glsl',
  'resources/glsl/es2_common.vs.glsl',
  'resources/glsl/gl3_common.fs.glsl',
  'resources/glsl/gl3_common.vs.glsl',
  'resources/glsl/gl_common.fs.glsl',
  'resources/glsl/gl_common.vs.glsl',
]

gsk_public_sources = files([
  'gskdiff.c',
  'gskcairorenderer.c',
  'gskrenderer.c',
  'gskrendernode.c',
  'gskrendernodeimpl.c',
  'gskrendernodeparser.c',
  'gskroundedrect.c',
  'gsktransform.c',
  'gl/gskglrenderer.c',
])

gsk_private_sources = files([
  'gskcairoblur.c',
  'gskdebug.c',
  'gskprivate.c',
  'gskprofiler.c',
  'gl/gskshaderbuilder.c',
  'gl/gskglprofiler.c',
  'gl/gskglglyphcache.c',
  'gl/gskglimage.c',
  'gl/gskgldriver.c',
  'gl/gskglrenderops.c',
  'gl/gskglshadowcache.c',
  'gl/gskglnodesample.c',
  'gl/stb_rect_pack.c',
])

gsk_public_headers = files([
  'gskcairorenderer.h',
  'gskenums.h',
  'gskrenderer.h',
  'gskrendernode.h',
  'gskroundedrect.h',
  'gsktransform.h',
  'gsktypes.h',
  'gsk-autocleanup.h',
])
install_headers(gsk_public_headers, 'gsk.h', subdir: 'gtk-4.0/gsk')

gsk_public_gl_headers = files([
  'gl/gskglrenderer.h'
])
install_headers(gsk_public_gl_headers, subdir: 'gtk-4.0/gsk/gl')
gsk_public_headers += gsk_public_gl_headers

gsk_public_broadway_headers = files([
  'broadway/gskbroadwayrenderer.h'
])
install_headers(gsk_public_broadway_headers, subdir: 'gtk-4.0/gsk/broadway')
gsk_public_headers += gsk_public_broadway_headers

gsk_public_vulkan_headers = files([
  'vulkan/gskvulkanrenderer.h'
])
install_headers(gsk_public_vulkan_headers, subdir: 'gtk-4.0/gsk/vulkan')
gsk_public_headers += gsk_public_vulkan_headers

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

  subdir('resources/vulkan')
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
                                     gsk_private_vulkan_compiled_shaders,
                                     gsk_private_vulkan_shaders
                                   ])

# FIXME: do we need this variable?
gsk_sources = gsk_public_sources + gsk_private_sources

gsk_enums = gnome.mkenums('gskenumtypes',
                          sources: gsk_public_headers,
                          c_template: 'gskenumtypes.c.template',
                          h_template: 'gskenumtypes.h.template',
                          install_dir: join_paths(gtk_includedir, 'gtk-4.0/gsk'),
                          install_header: true)

gskenum_h = gsk_enums[1]

gskresources = gnome.compile_resources('gskresources',
                                       gsk_resources_xml,
                                       dependencies: gsk_private_vulkan_compiled_shaders_deps,
                                       source_dir: '.',
                                       c_name: '_gsk',
                                       extra_args: [ '--manual-register', ])

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
                        sources: [ gsk_sources, gsk_enums, gskresources, ],
                        dependencies: gsk_deps,
                        include_directories: [ confinc, ],
                        c_args: [
                          '-DGSK_COMPILATION',
                          '-DG_LOG_DOMAIN="Gsk"',
                          '-DG_LOG_STRUCTURED=1',
                        ] + common_cflags,
                        link_with: libgdk,
                        link_args: common_ldflags)

# We don't have link_with: to internal static libs here on purpose, just
# list the dependencies and generated headers and such, for use in the
# "public" libgtk_dep used by internal executables.
libgsk_dep = declare_dependency(include_directories: [ confinc, ],
                                sources: [ gskenum_h, ],
                                dependencies: libgdk_dep)
