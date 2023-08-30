gsk_private_gpu_include_shaders = files([
  'color.glsl',
  'common.glsl',
  'common-gl.glsl',
  'common-vulkan.glsl',
  'ellipse.glsl',
  'enums.glsl',
  'pattern.glsl',
  'rect.glsl',
  'roundedrect.glsl',
])

gsk_private_gpu_shaders = files([
  'gskgpuuber.glsl',
  'gskgputexture.glsl',
])

gsk_private_gpu_shader_headers = []
gsk_private_gpu_gl_shaders = []
gsk_private_gpu_vulkan_vertex_shaders = []
gsk_private_gpu_vulkan_fragment_shaders = []

generate_header = find_program('generate-header.py')
process_includes = find_program('process-glsl-includes.py')

foreach shader: gsk_private_gpu_shaders
  instance = fs.name (fs.replace_suffix (shader, '')) + 'instance.h'
  shader_header = custom_target(instance,
                                output: instance,
                                input: shader,
                                command: [
                                  generate_header,
                                  '@INPUT@',
                                ],
                                capture: true)
  gsk_private_gpu_shader_headers += shader_header

  gl_shader_name = fs.name (shader)
  gl_shader = custom_target (gl_shader_name,
                             output: gl_shader_name,
                             input: shader,
                             depend_files: gsk_private_gpu_include_shaders,
                             command: [
                               process_includes,
                               '@INPUT@',
                             ],
                             capture: true)
  gsk_private_gpu_gl_shaders += gl_shader

  if (have_vulkan)
    vert_spv = fs.name (fs.replace_suffix (shader, '')) + '.vert.spv'
    frag_spv = fs.name (fs.replace_suffix (shader, '')) + '.frag.spv'
    vert_target = custom_target(vert_spv,
                                input: shader,
                                output: vert_spv,
                                depend_files: gsk_private_gpu_include_shaders,
                                command: [
                                  glslc,
                                  '-std=450',
                                  '--target-env=vulkan1.2',
                                  '-fshader-stage=vertex',
                                  '-DGSK_VERTEX_SHADER=1',
                                  '-O',
                                  '@INPUT@',
                                  '-o', '@OUTPUT@'
                                ])
    frag_target = custom_target(frag_spv,
                                input: shader,
                                output: frag_spv,
                                depend_files: gsk_private_gpu_include_shaders,
                                command: [
                                  glslc,
                                  '-std=450',
                                  '--target-env=vulkan1.2',
                                  '-fshader-stage=fragment',
                                  '-DGSK_FRAGMENT_SHADER=1',
                                  '-O',
                                  '@INPUT@',
                                  '-o', '@OUTPUT@'
                                ])
    gsk_private_gpu_vulkan_vertex_shaders += vert_target
    gsk_private_gpu_vulkan_fragment_shaders += frag_target
  endif
endforeach

