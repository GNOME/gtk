# FIXME: what's up with these?
#gsk_private_vulkan_include_shaders = [
#  'clip.frag.glsl',
#  'clip.vert.glsl',
#  'constants.glsl',
#  'rounded-rect.glsl',
#]

gsk_private_vulkan_fragment_shaders = [
  'blendmode.frag',
  'blur.frag',
  'border.frag',
  'color.frag',
  'color-matrix.frag',
  'crossfade.frag',
  'inset-shadow.frag',
  'linear.frag',
  'mask.frag',
  'outset-shadow.frag',
  'texture.frag',
]

gsk_private_vulkan_vertex_shaders = [
  'blendmode.vert',
  'blur.vert',
  'border.vert',
  'color.vert',
  'color-matrix.vert',
  'crossfade.vert',
  'inset-shadow.vert',
  'linear.vert',
  'mask.vert',
  'outset-shadow.vert',
  'texture.vert',
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
                                                 command: [
                                                   glslc,
                                                   stage_arg,
                                                   '-DCLIP_ROUNDED_RECT',
                                                   '@INPUT@',
                                                   '-o', '@OUTPUT@'
                                                 ])
    gsk_private_vulkan_compiled_shaders_deps += [compiled_shader, compiled_clip_shader, compiled_clip_rounded_shader]
  endif
  gsk_private_vulkan_compiled_shaders += files(spv_shader, clip_spv_shader, clip_rounded_spv_shader)
endforeach
