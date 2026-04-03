#include "config.h"

#include "gskvulkanpipelineprivate.h"

#include "gskgpucacheprivate.h"
#include "gskgpucachedprivate.h"
#include "gskgpushaderflagsprivate.h"
#include "gskgpushaderopprivate.h"

#include <glslang/Include/glslang_c_interface.h>

#include "gdkprofilerprivate.h"

typedef struct _GskVulkanShaderSpecialization GskVulkanShaderSpecialization;

struct _GskVulkanPipeline
{
  GskGpuCached parent;

  const GskGpuShaderOpClass *op_class;
  GskGpuShaderFlags flags;
  GskGpuColorStates color_states;
  guint32 variation;
  GskGpuBlend blend;
  VkFormat vk_format;
  VkPipelineLayout vk_layout;

  VkShaderModule vk_vertex_shader;
  VkShaderModule vk_fragment_shader;
  VkPipeline vk_pipeline;
};

struct _GskVulkanShaderSpecialization
{
  guint32 flags;
  guint32 color_states;
  guint32 variation;
};

static VkPipelineColorBlendAttachmentState blend_attachment_states[] = {
  [GSK_GPU_BLEND_NONE] = {
    .blendEnable = VK_FALSE,
    .colorWriteMask = VK_COLOR_COMPONENT_A_BIT
                    | VK_COLOR_COMPONENT_R_BIT
                    | VK_COLOR_COMPONENT_G_BIT
                    | VK_COLOR_COMPONENT_B_BIT
  },
  [GSK_GPU_BLEND_OVER] = {
    .blendEnable = VK_TRUE,
    .colorBlendOp = VK_BLEND_OP_ADD,
    .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
    .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    .alphaBlendOp = VK_BLEND_OP_ADD,
    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    .colorWriteMask = VK_COLOR_COMPONENT_A_BIT
                    | VK_COLOR_COMPONENT_R_BIT
                    | VK_COLOR_COMPONENT_G_BIT
                    | VK_COLOR_COMPONENT_B_BIT
  },
  [GSK_GPU_BLEND_ADD] = {
    .blendEnable = VK_TRUE,
    .colorBlendOp = VK_BLEND_OP_ADD,
    .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
    .dstColorBlendFactor = VK_BLEND_FACTOR_ONE,
    .alphaBlendOp = VK_BLEND_OP_ADD,
    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
    .colorWriteMask = VK_COLOR_COMPONENT_A_BIT
                    | VK_COLOR_COMPONENT_R_BIT
                    | VK_COLOR_COMPONENT_G_BIT
                    | VK_COLOR_COMPONENT_B_BIT
  },
  [GSK_GPU_BLEND_CLEAR] = {
    .blendEnable = VK_TRUE,
    .colorBlendOp = VK_BLEND_OP_ADD,
    .srcColorBlendFactor = VK_BLEND_FACTOR_ZERO,
    .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    .alphaBlendOp = VK_BLEND_OP_ADD,
    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
    .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
    .colorWriteMask = VK_COLOR_COMPONENT_A_BIT
                    | VK_COLOR_COMPONENT_R_BIT
                    | VK_COLOR_COMPONENT_G_BIT
                    | VK_COLOR_COMPONENT_B_BIT
  },
  [GSK_GPU_BLEND_MASK] = {
    .blendEnable = VK_TRUE,
    .colorBlendOp = VK_BLEND_OP_ADD,
    .srcColorBlendFactor = VK_BLEND_FACTOR_ZERO,
    .dstColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
    .alphaBlendOp = VK_BLEND_OP_ADD,
    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
    .dstAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
    .colorWriteMask = VK_COLOR_COMPONENT_A_BIT
                    | VK_COLOR_COMPONENT_R_BIT
                    | VK_COLOR_COMPONENT_G_BIT
                    | VK_COLOR_COMPONENT_B_BIT
  },
  [GSK_GPU_BLEND_MASK_ONE] = {
    .blendEnable = VK_TRUE,
    .colorBlendOp = VK_BLEND_OP_ADD,
    .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
    .dstColorBlendFactor = VK_BLEND_FACTOR_SRC1_ALPHA,
    .alphaBlendOp = VK_BLEND_OP_ADD,
    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
    .dstAlphaBlendFactor = VK_BLEND_FACTOR_SRC1_ALPHA,
    .colorWriteMask = VK_COLOR_COMPONENT_A_BIT
                    | VK_COLOR_COMPONENT_R_BIT
                    | VK_COLOR_COMPONENT_G_BIT
                    | VK_COLOR_COMPONENT_B_BIT
  },
  [GSK_GPU_BLEND_MASK_ALPHA] = {
    .blendEnable = VK_TRUE,
    .colorBlendOp = VK_BLEND_OP_ADD,
    .srcColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA,
    .dstColorBlendFactor = VK_BLEND_FACTOR_SRC1_ALPHA,
    .alphaBlendOp = VK_BLEND_OP_ADD,
    .srcAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA,
    .dstAlphaBlendFactor = VK_BLEND_FACTOR_SRC1_ALPHA,
    .colorWriteMask = VK_COLOR_COMPONENT_A_BIT
                    | VK_COLOR_COMPONENT_R_BIT
                    | VK_COLOR_COMPONENT_G_BIT
                    | VK_COLOR_COMPONENT_B_BIT
  },
  [GSK_GPU_BLEND_MASK_INV_ALPHA] = {
    .blendEnable = VK_TRUE,
    .colorBlendOp = VK_BLEND_OP_ADD,
    .srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
    .dstColorBlendFactor = VK_BLEND_FACTOR_SRC1_ALPHA,
    .alphaBlendOp = VK_BLEND_OP_ADD,
    .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
    .dstAlphaBlendFactor = VK_BLEND_FACTOR_SRC1_ALPHA,
    .colorWriteMask = VK_COLOR_COMPONENT_A_BIT
                    | VK_COLOR_COMPONENT_R_BIT
                    | VK_COLOR_COMPONENT_G_BIT
                    | VK_COLOR_COMPONENT_B_BIT
  },
};

static void
gsk_vulkan_pipeline_finalize (GskGpuCached *cached)
{
  GskGpuCachePrivate *priv = gsk_gpu_cache_get_private (cached->cache);
  GskVulkanPipeline *self = (GskVulkanPipeline *) cached;
  GskVulkanDevice *device;
  VkDevice vk_device;

  device = GSK_VULKAN_DEVICE (gsk_gpu_cache_get_device (cached->cache));
  vk_device = gsk_vulkan_device_get_vk_device (device);

  g_hash_table_remove (priv->pipeline_cache, self);

  vkDestroyPipeline (vk_device,
                     self->vk_pipeline,
                     NULL);
  vkDestroyShaderModule (vk_device,
                         self->vk_vertex_shader,
                         NULL);
  vkDestroyShaderModule (vk_device,
                         self->vk_fragment_shader,
                         NULL);
}

static gboolean
gsk_vulkan_pipeline_should_collect (GskGpuCached *cached,
                                    gint64        cache_timeout,
                                    gint64        timestamp)
{
  return gsk_gpu_cached_is_old (cached, cache_timeout, timestamp);
}

static const GskGpuCachedClass GSK_VULKAN_PIPELINE_CLASS =
{
  sizeof (GskVulkanPipeline),
  "VkPipeline",
  FALSE,
  gsk_gpu_cached_print_no_stats,
  gsk_vulkan_pipeline_finalize,
  gsk_vulkan_pipeline_should_collect
};

static guint
gsk_vulkan_pipeline_hash (gconstpointer data)
{
  const GskVulkanPipeline *self = data;

  return GPOINTER_TO_UINT (self->op_class) ^
         self->flags ^
         (self->color_states << 8) ^
         (self->variation << 16) ^
         (self->blend << 24) ^
         GPOINTER_TO_UINT (self->vk_layout) ^
         (self->vk_format << 21) ^ (self->vk_format >> 11);
}

static gboolean
gsk_vulkan_pipeline_equal (gconstpointer a,
                           gconstpointer b)
{
  const GskVulkanPipeline *selfa = a;
  const GskVulkanPipeline *selfb = b;

  return selfa->op_class == selfb->op_class &&
         selfa->flags == selfb->flags &&
         selfa->color_states == selfb->color_states &&
         selfa->variation == selfb->variation &&
         selfa->blend == selfb->blend &&
         selfa->vk_layout == selfb->vk_layout &&
         selfa->vk_format == selfb->vk_format;
}

void
gsk_vulkan_pipeline_init_cache (GskGpuCache *cache)
{
  GskGpuCachePrivate *priv = gsk_gpu_cache_get_private (cache);

  priv->pipeline_cache = g_hash_table_new (gsk_vulkan_pipeline_hash, gsk_vulkan_pipeline_equal);
}

void
gsk_vulkan_pipeline_finish_cache (GskGpuCache *cache)
{
  GskGpuCachePrivate *priv = gsk_gpu_cache_get_private (cache);

  g_assert (g_hash_table_size (priv->pipeline_cache) == 0);
  g_hash_table_unref (priv->pipeline_cache);
}

static VkShaderModule
gsk_vulkan_pipeline_create_vk_shader_module (GdkDisplay *display,
                                             const void *data,
                                             gsize       size_in_bytes)
{
  VkShaderModule result;

  if (GSK_VK_CHECK (vkCreateShaderModule, display->vk_device,
                                          &(VkShaderModuleCreateInfo) {
                                              .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                              .codeSize = size_in_bytes,
                                              .pCode = (uint32_t *) data,
                                          },
                                          NULL,
                                          &result) != VK_SUCCESS)
    {
      result = VK_NULL_HANDLE;
    }

  return result;
}

static glslang_spv_options_t *
gsk_glslang_get_default_spv_options (GdkDisplay *display)
{
  static glslang_spv_options_t options = {
    .generate_debug_info = false,
    .strip_debug_info = true,
    .disable_optimizer = false,
    .optimize_size = false,
    .disassemble = false,
    .validate = false,
    .emit_nonsemantic_shader_debug_info = false,
    .emit_nonsemantic_shader_debug_source = false,
    .compile_only = false,
  };

  return &options;
}

static const glslang_resource_t resource_limits = {
    .max_lights = 32,
    .max_clip_planes = 6,
    .max_texture_units = 32,
    .max_texture_coords = 32,
    .max_vertex_attribs = 64,
    .max_vertex_uniform_components = 4096,
    .max_varying_floats = 64,
    .max_vertex_texture_image_units = 32,
    .max_combined_texture_image_units = 80,
    .max_texture_image_units = 32,
    .max_fragment_uniform_components = 4096,
    .max_draw_buffers = 32,
    .max_vertex_uniform_vectors = 128,
    .max_varying_vectors = 8,
    .max_fragment_uniform_vectors = 16,
    .max_vertex_output_vectors = 16,
    .max_fragment_input_vectors = 15,
    .min_program_texel_offset = -8,
    .max_program_texel_offset = 7,
    .max_clip_distances = 8,
    .max_compute_work_group_count_x = 65535,
    .max_compute_work_group_count_y = 65535,
    .max_compute_work_group_count_z = 65535,
    .max_compute_work_group_size_x = 1024,
    .max_compute_work_group_size_y = 1024,
    .max_compute_work_group_size_z = 64,
    .max_compute_uniform_components = 1024,
    .max_compute_texture_image_units = 16,
    .max_compute_image_uniforms = 8,
    .max_compute_atomic_counters = 8,
    .max_compute_atomic_counter_buffers = 1,
    .max_varying_components = 60,
    .max_vertex_output_components = 64,
    .max_geometry_input_components = 64,
    .max_geometry_output_components = 128,
    .max_fragment_input_components = 128,
    .max_image_units = 8,
    .max_combined_image_units_and_fragment_outputs = 8,
    .max_combined_shader_output_resources = 8,
    .max_image_samples = 0,
    .max_vertex_image_uniforms = 0,
    .max_tess_control_image_uniforms = 0,
    .max_tess_evaluation_image_uniforms = 0,
    .max_geometry_image_uniforms = 0,
    .max_fragment_image_uniforms = 8,
    .max_combined_image_uniforms = 8,
    .max_geometry_texture_image_units = 16,
    .max_geometry_output_vertices = 256,
    .max_geometry_total_output_components = 1024,
    .max_geometry_uniform_components = 1024,
    .max_geometry_varying_components = 64,
    .max_tess_control_input_components = 128,
    .max_tess_control_output_components = 128,
    .max_tess_control_texture_image_units = 16,
    .max_tess_control_uniform_components = 1024,
    .max_tess_control_total_output_components = 4096,
    .max_tess_evaluation_input_components = 128,
    .max_tess_evaluation_output_components = 128,
    .max_tess_evaluation_texture_image_units = 16,
    .max_tess_evaluation_uniform_components = 1024,
    .max_tess_patch_components = 120,
    .max_patch_vertices = 32,
    .max_tess_gen_level = 64,
    .max_viewports = 16,
    .max_vertex_atomic_counters = 0,
    .max_tess_control_atomic_counters = 0,
    .max_tess_evaluation_atomic_counters = 0,
    .max_geometry_atomic_counters = 0,
    .max_fragment_atomic_counters = 8,
    .max_combined_atomic_counters = 8,
    .max_atomic_counter_bindings = 1,
    .max_vertex_atomic_counter_buffers = 0,
    .max_tess_control_atomic_counter_buffers = 0,
    .max_tess_evaluation_atomic_counter_buffers = 0,
    .max_geometry_atomic_counter_buffers = 0,
    .max_fragment_atomic_counter_buffers = 1,
    .max_combined_atomic_counter_buffers = 1,
    .max_atomic_counter_buffer_size = 16384,
    .max_transform_feedback_buffers = 4,
    .max_transform_feedback_interleaved_components = 64,
    .max_cull_distances = 8,
    .max_combined_clip_and_cull_distances = 8,
    .max_samples = 4,
    .max_mesh_output_vertices_nv = 256,
    .max_mesh_output_primitives_nv = 512,
    .max_mesh_work_group_size_x_nv = 32,
    .max_mesh_work_group_size_y_nv = 1,
    .max_mesh_work_group_size_z_nv = 1,
    .max_task_work_group_size_x_nv = 32,
    .max_task_work_group_size_y_nv = 1,
    .max_task_work_group_size_z_nv = 1,
    .max_mesh_view_count_nv = 4,
    .maxDualSourceDrawBuffersEXT = 1,

    .limits = {
        .non_inductive_for_loops = 1,
        .while_loops = 1,
        .do_while_loops = 1,
        .general_uniform_indexing = 1,
        .general_attribute_matrix_vector_indexing = 1,
        .general_varying_indexing = 1,
        .general_sampler_indexing = 1,
        .general_variable_indexing = 1,
        .general_constant_matrix_vector_indexing = 1,
     }
};

static char *
gsk_vulkan_pipeline_create_source_code (GdkDisplay        *display,
                                        GskGpuShaderFlags  flags,
                                        glslang_stage_t    stage,
                                        const char        *code)
{
  GString *s;

  s = g_string_new (NULL);

  g_string_append (s, "#extension GL_GOOGLE_include_directive : enable\n");
  switch ((int) stage)
  {
    case GLSLANG_STAGE_VERTEX:
      g_string_append (s, "#define GSK_VERTEX_SHADER 1\n");
      break;
    case GLSLANG_STAGE_FRAGMENT:
      g_string_append (s, "#define GSK_FRAGMENT_SHADER 1\n");
      break;
    default:
      g_assert_not_reached ();
      break;
  }
  if (gsk_gpu_shader_flags_has_clip_mask (flags))
    g_string_append (s, "#define GSK_VULKAN_HAS_CLIP_MASK\n");
  g_string_append (s, code);

  return g_string_free (s, FALSE);
}

static glsl_include_result_t *
gsk_vulkan_pipeline_handle_include (void       *ctx,
                                    const char *header_name,
                                    const char *includer_name,
                                    size_t      include_depth)
{
  char *resource_name;
  GError *error = NULL;
  glsl_include_result_t *result;
  GBytes *bytes;

  resource_name = g_strconcat ("/org/gtk/libgsk/shaders/sources/",
                               header_name,
                               NULL);

  bytes = g_resources_lookup_data (resource_name, 0, &error);

  if (bytes == NULL)
    {
      GDK_DEBUG (VULKAN, "Error loading shader data: %s", error->message);
      g_clear_error (&error);
      return VK_NULL_HANDLE;
    }

  result = g_new (glsl_include_result_t, 1);

  result->header_name = resource_name;
  result->header_data = g_bytes_get_data (bytes, NULL);
  result->header_length = g_bytes_get_size (bytes);

  return result;
}

static int
gsk_vulkan_pipeline_free_include (void *ctx,
                                  glsl_include_result_t *result)
{
  g_free ((char *) result->header_name);

  g_free (result);

  return 0;
}

static VkShaderModule
gsk_vulkan_pipeline_create_vk_shader_module_for_code (GdkDisplay        *display,
                                                      glslang_stage_t    stage,
                                                      const char        *code)
{
  const glslang_input_t input = {
    .language = GLSLANG_SOURCE_GLSL,
    .stage = stage,
    .client = GLSLANG_CLIENT_VULKAN,
    .client_version = GLSLANG_TARGET_VULKAN_1_0,
    .target_language = GLSLANG_TARGET_SPV,
    .target_language_version = GLSLANG_TARGET_SPV_1_0,
    .code = code,
    .default_version = 450,
    .default_profile = GLSLANG_NO_PROFILE,
    .force_default_version_and_profile = FALSE,
    .forward_compatible = FALSE,
    .messages = GLSLANG_MSG_DEFAULT_BIT,
    .resource = &resource_limits,
    .callbacks = {
        .include_system = gsk_vulkan_pipeline_handle_include,
        .include_local = gsk_vulkan_pipeline_handle_include,
        .free_include_result = gsk_vulkan_pipeline_free_include,
    },
  };
  glslang_shader_t *shader;
  glslang_program_t *program;
  VkShaderModule result;
  const char *messages;

  shader = glslang_shader_create (&input);
  if (shader == NULL)
    {
      g_assert_not_reached ();
      return NULL;
    }

  if (!glslang_shader_preprocess (shader, &input))
    {
      g_critical ("%s\n%s",
                  glslang_shader_get_info_log (shader),
                  glslang_shader_get_info_debug_log (shader));
      g_assert_not_reached ();
      glslang_shader_delete (shader);
      return NULL;
    }

  if (!glslang_shader_parse (shader, &input))
    {
      g_critical ("%s\n%s",
                  glslang_shader_get_info_log (shader),
                  glslang_shader_get_info_debug_log (shader));
      glslang_shader_delete (shader);
      return NULL;
    }

  program = glslang_program_create ();
  if (program == NULL)
    {
      g_assert_not_reached ();
      glslang_shader_delete (shader);
      return NULL;
    }

  glslang_program_add_shader (program, shader);

  if (!glslang_program_link (program, GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT))
    {
      g_critical ("%s\n%s",
                  glslang_program_get_info_log (program),
                  glslang_program_get_info_debug_log (program));
      glslang_program_delete (program);
      glslang_shader_delete (shader);
      return NULL;
    }

  glslang_program_SPIRV_generate_with_options (program,
                                               stage,
                                               gsk_glslang_get_default_spv_options (display));

  messages = glslang_program_SPIRV_get_messages (program);
  if (messages) {
    g_warning ("%s", messages);
  }

  result = gsk_vulkan_pipeline_create_vk_shader_module (display,
                                                        glslang_program_SPIRV_get_ptr (program),
                                                        glslang_program_SPIRV_get_size (program) * sizeof (unsigned int));

  glslang_program_delete (program);
  glslang_shader_delete (shader);

  return result;
}

static VkShaderModule
gsk_vulkan_pipeline_create_vk_shader_module_for_glsl (GdkDisplay        *display,
                                                      GskGpuShaderFlags  flags,
                                                      glslang_stage_t    stage,
                                                      const char        *resource_name)
{
  VkShaderModule shader;
  GError *error = NULL;
  GBytes *bytes;
  char *code;

  bytes = g_resources_lookup_data (resource_name, 0, &error);

  if (bytes == NULL)
    {
      GDK_DEBUG (VULKAN, "Error loading shader data: %s", error->message);
      g_clear_error (&error);
      return VK_NULL_HANDLE;
    }

  code = gsk_vulkan_pipeline_create_source_code (display, flags, stage, g_bytes_get_data (bytes, NULL));

  shader = gsk_vulkan_pipeline_create_vk_shader_module_for_code (display,
                                                                 stage,
                                                                 code);

  g_free (code);
  g_bytes_unref (bytes);

  return shader;
}

GskVulkanPipeline *
gsk_vulkan_pipeline_get (GskVulkanDevice           *device,
                         VkPipelineLayout           vk_layout,
                         const GskGpuShaderOpClass *op_class,
                         GskGpuShaderFlags          flags,
                         GskGpuColorStates          color_states,
                         guint32                    variation,
                         GskGpuBlend                blend,
                         VkFormat                   vk_format,
                         VkRenderPass               render_pass)
{
  GskGpuCache *cache = gsk_gpu_device_get_cache (GSK_GPU_DEVICE (device));
  GskGpuCachePrivate *priv = gsk_gpu_cache_get_private (cache);
  GskVulkanPipeline *result;
  GdkDisplay *display;
  G_GNUC_UNUSED gint64 begin_time = GDK_PROFILER_CURRENT_TIME;
  const char *blend_name[] = { "NONE", "OVER", "ADD", "CLEAR", "MASK", "MASK-ONE", "MASK-ALPHA", "MASK-INV-ALPHA" };
  char *shader_name;

  result = g_hash_table_lookup (priv->pipeline_cache,
                                &(GskVulkanPipeline) {
                                    .vk_layout = vk_layout,
                                    .op_class = op_class,
                                    .color_states = color_states,
                                    .variation = variation,
                                    .flags = flags,
                                    .blend = blend,
                                    .vk_format = vk_format,
                                });
  if (result)
    {
      gsk_gpu_cached_use ((GskGpuCached *) result);
      return result;
    }

  g_assert (blend <= G_N_ELEMENTS (blend_attachment_states));

  display = gsk_gpu_device_get_display (GSK_GPU_DEVICE (device));

  result = gsk_gpu_cached_new (cache, &GSK_VULKAN_PIPELINE_CLASS);
  result->vk_layout = vk_layout;
  result->op_class = op_class;
  result->color_states = color_states;
  result->variation = variation;
  result->flags = flags;
  result->blend = blend;
  result->vk_format = vk_format;

  shader_name = g_strconcat ("/org/gtk/libgsk/shaders/sources/",
                             op_class->shader_name,
                             ".glsl",
                             NULL);

  result->vk_vertex_shader = gsk_vulkan_pipeline_create_vk_shader_module_for_glsl (display,
                                                                                   flags,
                                                                                   GLSLANG_STAGE_VERTEX,
                                                                                   shader_name);
  result->vk_fragment_shader = gsk_vulkan_pipeline_create_vk_shader_module_for_glsl (display,
                                                                                     flags,
                                                                                     GLSLANG_STAGE_FRAGMENT,
                                                                                     shader_name);

  g_free (shader_name);

  GSK_VK_CHECK (vkCreateGraphicsPipelines, display->vk_device,
                                           display->vk_pipeline_cache,
                                           1,
                                           &(VkGraphicsPipelineCreateInfo) {
                                               .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                                               .stageCount = 2,
                                               .pStages = (VkPipelineShaderStageCreateInfo[2]) {
                                                   {
                                                       .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                       .stage = VK_SHADER_STAGE_VERTEX_BIT,
                                                       .module = result->vk_vertex_shader,
                                                       .pName = "main",
                                                       .pSpecializationInfo = &(VkSpecializationInfo) {
                                                           .mapEntryCount = 3,
                                                           .pMapEntries = (VkSpecializationMapEntry[6]) {
                                                               {
                                                                   .constantID = 0,
                                                                   .offset = G_STRUCT_OFFSET (GskVulkanShaderSpecialization, flags),
                                                                   .size = sizeof (guint32),
                                                               },
                                                               {
                                                                   .constantID = 1,
                                                                   .offset = G_STRUCT_OFFSET (GskVulkanShaderSpecialization, color_states),
                                                                   .size = sizeof (guint32),
                                                               },
                                                               {
                                                                   .constantID = 2,
                                                                   .offset = G_STRUCT_OFFSET (GskVulkanShaderSpecialization, variation),
                                                                   .size = sizeof (guint32),
                                                               },
                                                           },
                                                           .dataSize = sizeof (GskVulkanShaderSpecialization),
                                                           .pData = &(GskVulkanShaderSpecialization) {
                                                               .flags = flags,
                                                               .color_states = color_states,
                                                               .variation = variation,
                                                           },
                                                       },
                                                   },
                                                   {
                                                       .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                       .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                       .module = result->vk_fragment_shader,
                                                       .pName = "main",
                                                       .pSpecializationInfo = &(VkSpecializationInfo) {
                                                           .mapEntryCount = 3,
                                                           .pMapEntries = (VkSpecializationMapEntry[6]) {
                                                               {
                                                                   .constantID = 0,
                                                                   .offset = G_STRUCT_OFFSET (GskVulkanShaderSpecialization, flags),
                                                                   .size = sizeof (guint32),
                                                               },
                                                               {
                                                                   .constantID = 1,
                                                                   .offset = G_STRUCT_OFFSET (GskVulkanShaderSpecialization, color_states),
                                                                   .size = sizeof (guint32),
                                                               },
                                                               {
                                                                   .constantID = 2,
                                                                   .offset = G_STRUCT_OFFSET (GskVulkanShaderSpecialization, variation),
                                                                   .size = sizeof (guint32),
                                                               },
                                                           },
                                                           .dataSize = sizeof (GskVulkanShaderSpecialization),
                                                           .pData = &(GskVulkanShaderSpecialization) {
                                                               .flags = flags,
                                                               .color_states = color_states,
                                                               .variation = variation,
                                                           },
                                                       },
                                                   },
                                               },

                                               .pVertexInputState = op_class->vertex_input_state,
                                               .pInputAssemblyState = &(VkPipelineInputAssemblyStateCreateInfo) {
                                                   .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                                                   .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                                                   .primitiveRestartEnable = VK_FALSE,
                                               },
                                               .pTessellationState = NULL,
                                               .pViewportState = &(VkPipelineViewportStateCreateInfo) {
                                                   .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                                                   .viewportCount = 1,
                                                   .scissorCount = 1
                                               },
                                               .pRasterizationState = &(VkPipelineRasterizationStateCreateInfo) {
                                                   .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                                                   .depthClampEnable = VK_FALSE,
                                                   .rasterizerDiscardEnable = VK_FALSE,
                                                   .polygonMode = VK_POLYGON_MODE_FILL,
                                                   .cullMode = VK_CULL_MODE_NONE,
                                                   .frontFace = VK_FRONT_FACE_CLOCKWISE,
                                                   .lineWidth = 1.0f,
                                               },
                                               .pMultisampleState = &(VkPipelineMultisampleStateCreateInfo) {
                                                   .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                                                   .rasterizationSamples = 1,
                                               },
                                               .pDepthStencilState = &(VkPipelineDepthStencilStateCreateInfo) {
                                                   .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO
                                               },
                                               .pColorBlendState = &(VkPipelineColorBlendStateCreateInfo) {
                                                   .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                                                   .attachmentCount = 1,
                                                   .pAttachments = &blend_attachment_states[blend],
                                               },
                                               .pDynamicState = &(VkPipelineDynamicStateCreateInfo) {
                                                   .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                                                   .dynamicStateCount = 2,
                                                   .pDynamicStates = (VkDynamicState[2]) {
                                                       VK_DYNAMIC_STATE_VIEWPORT,
                                                       VK_DYNAMIC_STATE_SCISSOR
                                                   },
                                               },
                                               .layout = vk_layout,
                                               .renderPass = render_pass,
                                               .subpass = 0,
                                               .basePipelineHandle = VK_NULL_HANDLE,
                                               .basePipelineIndex = -1,
                                           },
                                           NULL,
                                           &result->vk_pipeline);

  gdk_profiler_end_markf (begin_time,
                          "Create Vulkan pipeline", "%s color states=%u variation=%u clip=%u blend=%s format=%u",
                          op_class->shader_name,
                          flags,
                          color_states, 
                          variation,
                          blend >= G_N_ELEMENTS (blend_name) ? "FIX THE BLEND NAMES ARRAY!" : blend_name[blend],
                          vk_format);

  GSK_DEBUG (SHADERS,
             "Create Vulkan pipeline (%s, %u/%u/%u/%s/%u)",
             op_class->shader_name,
             flags,
             color_states, 
             variation,
             blend >= G_N_ELEMENTS (blend_name) ? "FIX THE BLEND NAMES ARRAY!" : blend_name[blend],
             vk_format);

  g_hash_table_add (priv->pipeline_cache, result);
  gdk_display_vulkan_pipeline_cache_updated (display);
  gsk_gpu_cached_use ((GskGpuCached *) result);

  return result;
}

VkPipeline
gsk_vulkan_pipeline_get_vk_pipeline (GskVulkanPipeline *self)
{
  return self->vk_pipeline;
}

