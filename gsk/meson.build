gsk_private_source_shaders = [
  'resources/glsl/blend.fs.glsl',
  'resources/glsl/blend.vs.glsl',
  'resources/glsl/blit.fs.glsl',
  'resources/glsl/blit.vs.glsl',
  'resources/glsl/color.fs.glsl',
  'resources/glsl/color.vs.glsl',
  'resources/glsl/es2_common.fs.glsl',
  'resources/glsl/es2_common.vs.glsl',
  'resources/glsl/gl3_common.fs.glsl',
  'resources/glsl/gl3_common.vs.glsl',
  'resources/glsl/gl_common.fs.glsl',
  'resources/glsl/gl_common.vs.glsl',
]

gsk_public_sources = files([
  'gskpixelshader.c',
  'gskrenderer.c',
  'gskrendernode.c',
  'gskrendernodeimpl.c',
  'gskroundedrect.c',
  'gskslcompiler.c',
  'gskslprogram.c',
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
  'gskslfunction.c',
  'gskslnode.c',
  'gskslpreprocessor.c',
  'gskslpointertype.c',
  'gskslscope.c',
  'gsksltokenizer.c',
  'gsksltype.c',
  'gskspvwriter.c'
])

gsk_public_headers = files([
  'gskenums.h',
  'gskpixelshader.h',
  'gskrenderer.h',
  'gskrendernode.h',
  'gskroundedrect.h',
  'gsktexture.h',
  'gsktypes.h',
])

install_headers(gsk_public_headers, 'gsk.h', subdir: 'gtk-4.0/gsk')

gsk_private_vulkan_shaders = []
# This is an odd split because we use configure_file() below to workaround
# a limitation in meson preventing using custom_target() with gnome.compile_resources()
# and that requires file paths, but we also need to have dependencies during development
# on constantly regenerated files.
gsk_private_vulkan_compiled_shaders = []
gsk_private_vulkan_compiled_shaders_deps = []

if have_vulkan
  gsk_private_sources += files([
    'gskvulkanblendmodepipeline.c',
    'gskvulkanblurpipeline.c',
    'gskvulkanborderpipeline.c',
    'gskvulkanboxshadowpipeline.c',
    'gskvulkanbuffer.c',
    'gskvulkanclip.c',
    'gskvulkancolorpipeline.c',
    'gskvulkancolortextpipeline.c',
    'gskvulkancrossfadepipeline.c',
    'gskvulkancommandpool.c',
    'gskvulkaneffectpipeline.c',
    'gskvulkanglyphcache.c',
    'gskvulkanlineargradientpipeline.c',
    'gskvulkanimage.c',
    'gskvulkantextpipeline.c',
    'gskvulkantexturepipeline.c',
    'gskvulkanmemory.c',
    'gskvulkanpipeline.c',
    'gskvulkanpushconstants.c',
    'gskvulkanrender.c',
    'gskvulkanrenderer.c',
    'gskvulkanrenderpass.c',
    'gskvulkanshader.c',
  ])

  subdir('resources/vulkan')
endif # have_vulkan

gsk_resources_xml = configure_file(output: 'gsk.resources.xml',
                                   input: 'gen-gsk-gresources-xml.py',
                                   command: [
                                     find_program('gen-gsk-gresources-xml.py'),
                                     '@OUTPUT@',
                                     gsk_private_source_shaders,
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
