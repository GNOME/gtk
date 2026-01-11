gsk_private_gpu_include_shaders = files([
  'blendmode.glsl',
  'color.glsl',
  'common.glsl',
  'common-gl.glsl',
  'common-vulkan.glsl',
  'ellipse.glsl',
  'enums.glsl',
  'rect.glsl',
  'roundedrect.glsl',
])

converted_shaders = files([
  'gskgpuarithmetic.glsl',
  'gskgpublendmode.glsl',
  'gskgpublur.glsl',
  'gskgpuborder.glsl',
  'gskgpuboxshadow.glsl',
  'gskgpucolor.glsl',
  'gskgpucolorize.glsl',
  'gskgpucolormatrix.glsl',
  'gskgpucomposite.glsl',
  'gskgpuconicgradient.glsl',
  'gskgpuconvert.glsl',
  'gskgpuconvertbuiltin.glsl',
  'gskgpuconvertcicp.glsl',
  'gskgpucrossfade.glsl',
  'gskgpudisplacement.glsl',
  'gskgpulineargradient.glsl',
  'gskgpumask.glsl',
  'gskgpuradialgradient.glsl',
  'gskgpuroundedcolor.glsl',
  'gskgputexture.glsl',
])

gsk_private_gpu_shaders = files([
  'gskgpucomponenttransfer.glsl',
]) + converted_shaders

gsk_private_gpu_shader_headers = []
gsk_generated_gpu_shader_sources = []
gsk_private_gpu_gl_shaders = []
gsk_private_gpu_vulkan_shaders = []

generate_interface = find_program('generate-glsl-interface.py')
process_includes = find_program('process-glsl-includes.py')

foreach shader: gsk_private_gpu_shaders
  if (converted_shaders.contains (shader))
    source_file = fs.name (fs.replace_suffix (shader, '')) + 'op.c'
    instance = fs.name (fs.replace_suffix (shader, '')) + 'opprivate.h'
    shader_source = custom_target(source_file,
                                  output: source_file,
                                  input: shader,
                                  command: [
                                    generate_interface,
                                    '--generate-source',
                                    '@INPUT@',
                                  ],
                                  capture: true)
  else
    instance = fs.name (fs.replace_suffix (shader, '')) + 'instance.h'
    shader_source = []
  endif
  glsl_instance = fs.name (fs.replace_suffix (shader, '')) + 'instance.glsl'
  shader_header = custom_target(instance,
                                output: instance,
                                input: shader,
                                command: [
                                  generate_interface,
                                  converted_shaders.contains (shader) 
                                  ? '--generate-header' 
                                  : '--generate-old-header',
                                  '@INPUT@',
                                ],
                                capture: true)
  shader_glsl = custom_target(glsl_instance,
                              output: glsl_instance,
                              input: shader,
                              command: [
                                generate_interface,
                                '--generate-glsl',
                                '@INPUT@',
                              ],
                              capture: true)

  gsk_private_gpu_shader_headers += shader_header
  gsk_generated_gpu_shader_sources += shader_source

  gl_shader_name = fs.name (shader)
  gl_shader = custom_target (gl_shader_name,
                             output: gl_shader_name,
                             input: shader,
                             depends: shader_glsl,
                             depend_files: gsk_private_gpu_include_shaders,
                             command: [
                               process_includes,
                               '-I'+meson.current_source_dir(),
                               '-I'+meson.current_build_dir(),
                               '@INPUT@',
                             ],
                             capture: true)
  gsk_private_gpu_gl_shaders += gl_shader

  if (have_vulkan)
    glslc_options = [
      [ fs.name (fs.replace_suffix (shader, '')) + '.vert.spv',
        [ '--target-env=vulkan1.0',
          '-fshader-stage=vertex',
          '-DGSK_VERTEX_SHADER=1',
          '-I'+meson.current_source_dir(),
          '-I'+meson.current_build_dir(),
        ]
      ],
      [ fs.name (fs.replace_suffix (shader, '')) + '.frag.spv',
        [ '--target-env=vulkan1.0',
          '-fshader-stage=fragment',
          '-DGSK_FRAGMENT_SHADER=1',
          '-I'+meson.current_source_dir(),
          '-I'+meson.current_build_dir(),
        ]
      ],
    ]
    foreach option: glslc_options
      target = custom_target(option.get(0),
                             input: shader,
                             output: option.get(0),
                             depends: shader_glsl,
                             depend_files: gsk_private_gpu_include_shaders,
                             command: [
                               glslc,
                               '-std=450',
                               option.get(1),
                               '@INPUT@',
                               '-o', '@OUTPUT@'
                             ])
      gsk_private_gpu_vulkan_shaders += target
    endforeach
  endif
endforeach

