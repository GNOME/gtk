gsk_public_sources = files([
  'gskarithmeticnode.c',
  'gskblendnode.c',
  'gskblurnode.c',
  'gskbordernode.c',
  'gskcaironode.c',
  'gskcairorenderer.c',
  'gskclipnode.c',
  'gskcolormatrixnode.c',
  'gskcolornode.c',
  'gskcomponenttransfer.c',
  'gskcomponenttransfernode.c',
  'gskcompositenode.c',
  'gskconicgradientnode.c',
  'gskcontainernode.c',
  'gskcopynode.c',
  'gskcopypasteutils.c',
  'gskcrossfadenode.c',
  'gskdebugnode.c',
  'gskdiff.c',
  'gskdisplacementnode.c',
  'gskfillnode.c',
  'gskglshader.c',
  'gskglshadernode.c',
  'gskgradient.c',
  'gskinsetshadownode.c',
  'gskisolationnode.c',
  'gsklineargradientnode.c',
  'gskmasknode.c',
  'gskoffload.c',
  'gskopacitynode.c',
  'gskoutsetshadownode.c',
  'gskpastenode.c',
  'gskpath.c',
  'gskpathbuilder.c',
  'gskpathintersect.c',
  'gskpathmeasure.c',
  'gskpathparser.c',
  'gskpathpoint.c',
  'gskradialgradientnode.c',
  'gskrenderer.c',
  'gskrendernode.c',
  'gskrendernodeimpl.c',
  'gskrendernodeparser.c',
  'gskrenderreplay.c',
  'gskrepeatnode.c',
  'gskroundedclipnode.c',
  'gskroundedrect.c',
  'gskshadownode.c',
  'gskstroke.c',
  'gskstrokenode.c',
  'gsksubsurfacenode.c',
  'gsktextnode.c',
  'gsktexturenode.c',
  'gsktexturescalenode.c',
  'gsktransform.c',
  'gsktransformnode.c',
  'gpu/gskd3d12renderer.c',
  'gpu/gskglrenderer.c',
  'gpu/gskvulkanrenderer.c',
])

gsk_private_sources = files([
  'gskcairoblur.c',
  'gskcairogradient.c',
  'gskcairoshadow.c',
  'gskcontour.c',
  'gskcurve.c',
  'gskcurveintersect.c',
  'gskdebug.c',
  'gskprivate.c',
  'gl/fp16.c',
  'gpu/gskglbuffer.c',
  'gpu/gskgldevice.c',
  'gpu/gskglframe.c',
  'gpu/gskglimage.c',
  'gpu/gskgpublendop.c',
  'gpu/gskgpublitop.c',
  'gpu/gskgpubuffer.c',
  'gpu/gskgpucache.c',
  'gpu/gskgpucachedfill.c',
  'gpu/gskgpucachedglyph.c',
  'gpu/gskgpucachedstroke.c',
  'gpu/gskgpuclearop.c',
  'gpu/gskgpuclip.c',
  'gpu/gskgpudownloadop.c',
  'gpu/gskgpudevice.c',
  'gpu/gskgpuframe.c',
  'gpu/gskgpuglobalsop.c',
  'gpu/gskgpuimage.c',
  'gpu/gskgpumipmapop.c',
  'gpu/gskgpunodeprocessor.c',
  'gpu/gskgpuop.c',
  'gpu/gskgpuprint.c',
  'gpu/gskgpurenderer.c',
  'gpu/gskgpurenderpassop.c',
  'gpu/gskgpushaderop.c',
  'gpu/gskgpuscissorop.c',
  'gpu/gskgpuutils.c',
  'gpu/gskgpuuploadop.c',
])

gsk_f16c_sources = files([
  'gl/fp16i.c',
])

gsk_public_headers = files([
  'gskblendnode.h',
  'gskblurnode.h',
  'gskbordernode.h',
  'gskcaironode.h',
  'gskcairorenderer.h',
  'gskclipnode.h',
  'gskcolormatrixnode.h',
  'gskcolornode.h',
  'gskcomponenttransfer.h',
  'gskcomponenttransfernode.h',
  'gskcompositenode.h',
  'gskconicgradientnode.h',
  'gskcontainernode.h',
  'gskcopynode.h',
  'gskcrossfadenode.h',
  'gskdebugnode.h',
  'gskenums.h',
  'gskfillnode.h',
  'gskglshader.h',
  'gskglshadernode.h',
  'gskinsetshadownode.h',
  'gskisolationnode.h',
  'gsklineargradientnode.h',
  'gskmasknode.h',
  'gskopacitynode.h',
  'gskoutsetshadownode.h',
  'gskpastenode.h',
  'gskpath.h',
  'gskpathbuilder.h',
  'gskpathmeasure.h',
  'gskpathpoint.h',
  'gskradialgradientnode.h',
  'gskrenderer.h',
  'gskrendernode.h',
  'gskrenderreplay.h',
  'gskrepeatnode.h',
  'gskroundedclipnode.h',
  'gskroundedrect.h',
  'gskshadownode.h',
  'gskstroke.h',
  'gskstrokenode.h',
  'gsksubsurfacenode.h',
  'gsktextnode.h',
  'gsktexturenode.h',
  'gsktexturescalenode.h',
  'gsktransform.h',
  'gsktransformnode.h',
  'gsktypes.h',
])
install_headers(gsk_public_headers, 'gsk.h', subdir: 'gtk-4.0/gsk')

gsk_public_gl_headers = files([
  'gl/gskglrenderer.h',
])
install_headers(gsk_public_gl_headers, subdir: 'gtk-4.0/gsk/gl')
gsk_public_headers += gsk_public_gl_headers

gsk_public_gpu_headers = files([
  'gpu/gskd3d12renderer.h',
  'gpu/gskglrenderer.h',
  'gpu/gskvulkanrenderer.h'
])
install_headers(gsk_public_gpu_headers, subdir: 'gtk-4.0/gsk/gpu')
gsk_public_headers += gsk_public_gpu_headers

if get_variable('broadway_enabled')
  gsk_public_broadway_headers = files([
    'broadway/gskbroadwayrenderer.h'
  ])
  install_headers(gsk_public_broadway_headers, subdir: 'gtk-4.0/gsk/broadway')
  gsk_public_headers += gsk_public_broadway_headers
endif

gsk_private_vulkan_shader_headers = []

if have_vulkan
  gsk_private_sources += files([
    'gpu/gskvulkanbuffer.c',
    'gpu/gskvulkandebugframe.c',
    'gpu/gskvulkandevice.c',
    'gpu/gskvulkanframe.c',
    'gpu/gskvulkanimage.c',
    'gpu/gskvulkanmemory.c',
    'gpu/gskvulkanycbcr.c',
  ])
endif # have_vulkan

if get_variable('win32_enabled')
  gsk_private_sources += files([
    'gpu/gskd3d12device.c',
    'gpu/gskd3d12frame.c',
    'gpu/gskd3d12image.c',
  ])
endif

subdir('gpu/shaders')

if get_variable('broadway_enabled')
  gsk_public_sources += files([
    'broadway/gskbroadwayrenderer.c',
  ])
endif

gsk_resources_xml = custom_target(output: 'gsk.resources.xml',
  input: 'gen-gsk-gresources-xml.py',
  command: [
    find_program('gen-gsk-gresources-xml.py'),
    '@OUTPUT@',
    gsk_private_gpu_gl_shaders,
    gsk_private_gpu_vulkan_shaders,
    gsk_private_gpu_d3d12_shaders,
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
  dependencies: [
                  gsk_private_gpu_gl_shaders,
                  gsk_private_gpu_vulkan_shaders,
                ],
  source_dir: [meson.current_build_dir(), meson.current_source_dir()],
  c_name: '_gsk',
  extra_args: [ '--manual-register', ],
)

gsk_gen_headers = [ gskenum_h, ]

gsk_deps = [
  graphene_dep,
  pango_dep,
  cairo_dep,
  cairo_csi_dep,
  libgdk_dep,
]

libgsk_f16c = static_library('gsk_f16c',
  sources: gsk_f16c_sources,
  dependencies: gsk_deps,
  include_directories: [ confinc, ],
  c_args: [
    '-DGTK_COMPILATION',
    '-DG_LOG_DOMAIN="Gsk"',
    '-DG_LOG_STRUCTURED=1',
  ] + common_cflags + f16c_cflags,
)

libgsk = static_library('gsk',
  sources: [
    gsk_public_sources,
    gsk_private_sources,
    gsk_enums,
    gskresources,
    gsk_private_vulkan_shader_headers,
    gsk_private_gpu_shader_headers,
    gsk_generated_gpu_shader_sources,
  ],
  dependencies: gsk_deps,
  include_directories: [ confinc, include_directories ('gpu') ],
  c_args: [
    '-DGTK_COMPILATION',
    '-DG_LOG_DOMAIN="Gsk"',
    '-DG_LOG_STRUCTURED=1',
  ] + common_cflags,
  link_with: [ libgdk, libgsk_f16c]
)

# We don't have link_with: to internal static libs here on purpose, just
# list the dependencies and generated headers and such, for use in the
# "public" libgtk_dep used by internal executables.
libgsk_dep = declare_dependency(include_directories: [ confinc, ],
  sources: [ gskenum_h, ],
  dependencies: [libgdk_dep, cairo_csi_dep],
)
