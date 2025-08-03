gsk_public_sources = files([
  'gskcairorenderer.c',
  'gskcomponenttransfer.c',
  'gskdiff.c',
  'gskglshader.c',
  'gskoffload.c',
  'gskpath.c',
  'gskpathbuilder.c',
  'gskpathintersect.c',
  'gskpathmeasure.c',
  'gskpathparser.c',
  'gskpathpoint.c',
  'gskrenderer.c',
  'gskrendernode.c',
  'gskrendernodeimpl.c',
  'gskrendernodeparser.c',
  'gskroundedrect.c',
  'gskstroke.c',
  'gsktransform.c',
  'gpu/gskglrenderer.c',
  'gpu/gskvulkanrenderer.c',
])

gsk_private_sources = files([
  'gskcairoblur.c',
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
  'gpu/gskgpublendmodeop.c',
  'gpu/gskgpublitop.c',
  'gpu/gskgpublurop.c',
  'gpu/gskgpuborderop.c',
  'gpu/gskgpuboxshadowop.c',
  'gpu/gskgpubuffer.c',
  'gpu/gskgpucache.c',
  'gpu/gskgpucachedfill.c',
  'gpu/gskgpucachedglyph.c',
  'gpu/gskgpucachedstroke.c',
  'gpu/gskgpuclearop.c',
  'gpu/gskgpuclip.c',
  'gpu/gskgpucolorizeop.c',
  'gpu/gskgpucolormatrixop.c',
  'gpu/gskgpucomponenttransferop.c',
  'gpu/gskgpucolorop.c',
  'gpu/gskgpuconicgradientop.c',
  'gpu/gskgpuconvertop.c',
  'gpu/gskgpuconvertbuiltinop.c',
  'gpu/gskgpuconvertcicpop.c',
  'gpu/gskgpucrossfadeop.c',
  'gpu/gskgpudownloadop.c',
  'gpu/gskgpudevice.c',
  'gpu/gskgpuframe.c',
  'gpu/gskgpuglobalsop.c',
  'gpu/gskgpuimage.c',
  'gpu/gskgpulineargradientop.c',
  'gpu/gskgpumaskop.c',
  'gpu/gskgpumipmapop.c',
  'gpu/gskgpunodeprocessor.c',
  'gpu/gskgpuop.c',
  'gpu/gskgpuprint.c',
  'gpu/gskgpuradialgradientop.c',
  'gpu/gskgpurenderer.c',
  'gpu/gskgpurenderpassop.c',
  'gpu/gskgpuroundedcolorop.c',
  'gpu/gskgpushaderop.c',
  'gpu/gskgpuscissorop.c',
  'gpu/gskgputextureop.c',
  'gpu/gskgpuutils.c',
  'gpu/gskgpuuploadop.c',
])

gsk_f16c_sources = files([
  'gl/fp16i.c',
])

gsk_public_headers = files([
  'gskcairorenderer.h',
  'gskcomponenttransfer.h',
  'gskenums.h',
  'gskglshader.h',
  'gskpath.h',
  'gskpathbuilder.h',
  'gskpathmeasure.h',
  'gskpathpoint.h',
  'gskrenderer.h',
  'gskrendernode.h',
  'gskroundedrect.h',
  'gskstroke.h',
  'gsktransform.h',
  'gsktypes.h',
])
install_headers(gsk_public_headers, 'gsk.h', subdir: 'gtk-4.0/gsk')

gsk_public_gl_headers = files([
  'gl/gskglrenderer.h',
])
install_headers(gsk_public_gl_headers, subdir: 'gtk-4.0/gsk/gl')
gsk_public_headers += gsk_public_gl_headers

gsk_public_gpu_headers = files([
  'gpu/gskvulkanrenderer.h',
  'gpu/gskglrenderer.h'
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

gsk_private_vulkan_shaders = []
gsk_private_vulkan_compiled_shaders = []
gsk_private_vulkan_compiled_shaders_deps = []
gsk_private_vulkan_shader_headers = []

if have_vulkan
  gsk_private_sources += files([
    'gpu/gskvulkanbuffer.c',
    'gpu/gskvulkandevice.c',
    'gpu/gskvulkanframe.c',
    'gpu/gskvulkanimage.c',
    'gpu/gskvulkanmemory.c',
    'gpu/gskvulkanycbcr.c',
  ])
endif # have_vulkan

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
    gsk_private_vulkan_compiled_shaders,
    gsk_private_vulkan_shaders,
    gsk_private_gpu_gl_shaders,
    gsk_private_gpu_vulkan_shaders,
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
                  gsk_private_vulkan_compiled_shaders_deps,
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
  ],
  dependencies: gsk_deps,
  include_directories: [ confinc, ],
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
