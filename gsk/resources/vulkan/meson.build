# FIXME: what's up with these?
#gsk_private_vulkan_include_shaders = [
#  'clip.frag.glsl',
#  'clip.vert.glsl',
#  'constants.glsl',
#  'rounded-rect.glsl',
#]

gsk_private_vulkan_fragment_shaders = [
  'blend.frag',
  'border.frag',
  'color.frag',
  'color-matrix.frag',
  'inset-shadow.frag',
  'linear.frag',
  'outset-shadow.frag',
]

gsk_private_vulkan_vertex_shaders = [
  'blend.vert',
  'border.vert',
  'color.vert',
  'color-matrix.vert',
  'inset-shadow.vert',
  'linear.vert',
  'outset-shadow.vert',
]

gsk_private_vulkan_shaders += gsk_private_vulkan_fragment_shaders
gsk_private_vulkan_shaders += gsk_private_vulkan_vertex_shaders

glslc = find_program('glslc', required: false)
foreach shader: gsk_private_vulkan_shaders
  basefn = shader.split('.').get(0)
  suffix = shader.split('.').get(1)

  stage_arg = suffix == 'frag' ? '-fshader-stage=fragment' : '-fshader-stage=vertex'
  spv_shader = '@0@.@1@.spv'.format(basefn, suffix)
  clip_spv_shader = '@0@-clip.@1@.spv'.format(basefn, suffix)
  clip_rounded_spv_shader = '@0@-clip-rounded.@1@.spv'.format(basefn, suffix)

  if glslc.found()
    compiled_shader = custom_target(spv_shader,
                                    input: shader,
                                    output: spv_shader,
                                    build_by_default: true,
                                    command: [
                                      glslc,
                                      stage_arg,
                                      '-DCLIP_NONE',
                                      '@INPUT@',
                                      '-o', '@OUTPUT@'
                                    ])
    compiled_clip_shader = custom_target(clip_spv_shader,
                                         input: shader,
                                         output: clip_spv_shader,
                                         build_by_default: true,
                                         command: [
                                           glslc,
                                           stage_arg,
                                           '-DCLIP_RECT',
                                           '@INPUT@',
                                           '-o', '@OUTPUT@'
                                         ])
    compiled_clip_rounded_shader = custom_target(clip_rounded_spv_shader,
                                                 input: shader,
                                                 output: clip_rounded_spv_shader,
                                                 build_by_default: true,
                                                 command: [
                                                   glslc,
                                                   stage_arg,
                                                   '-DCLIP_ROUNDED_RECT',
                                                   '@INPUT@',
                                                   '-o', '@OUTPUT@'
                                                 ])
  endif
  gsk_private_vulkan_compiled_shaders += files(spv_shader)
  gsk_private_vulkan_compiled_shaders += files(clip_spv_shader)
  gsk_private_vulkan_compiled_shaders += files(clip_rounded_spv_shader)
endforeach
