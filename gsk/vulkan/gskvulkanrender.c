#include "config.h"

#include "gskprivate.h"

#include "gskvulkanrenderprivate.h"

#include "gskrendererprivate.h"
#include "gskvulkanbufferprivate.h"
#include "gskvulkancommandpoolprivate.h"
#include "gskvulkandownloadopprivate.h"
#include "gskvulkanglyphcacheprivate.h"
#include "gskvulkanprivate.h"
#include "gskvulkanpushconstantsopprivate.h"
#include "gskvulkanrendererprivate.h"
#include "gskvulkanrenderpassprivate.h"
#include "gskvulkanrenderpassopprivate.h"
#include "gskvulkanshaderopprivate.h"

#include "gdk/gdkvulkancontextprivate.h"

#define GDK_ARRAY_NAME gsk_vulkan_render_ops
#define GDK_ARRAY_TYPE_NAME GskVulkanRenderOps
#define GDK_ARRAY_ELEMENT_TYPE guchar
#define GDK_ARRAY_BY_VALUE 1
#include "gdk/gdkarrayimpl.c"

#define DESCRIPTOR_POOL_MAXITEMS 50000
#define VERTEX_BUFFER_SIZE_STEP 128 * 1024 /* 128kB */

#define GDK_ARRAY_NAME gsk_descriptor_image_infos
#define GDK_ARRAY_TYPE_NAME GskDescriptorImageInfos
#define GDK_ARRAY_ELEMENT_TYPE VkDescriptorImageInfo
#define GDK_ARRAY_BY_VALUE 1
#define GDK_ARRAY_PREALLOC 1024
#define GDK_ARRAY_NO_MEMSET 1
#include "gdk/gdkarrayimpl.c"

#define GDK_ARRAY_NAME gsk_descriptor_buffer_infos
#define GDK_ARRAY_TYPE_NAME GskDescriptorBufferInfos
#define GDK_ARRAY_ELEMENT_TYPE VkDescriptorBufferInfo
#define GDK_ARRAY_BY_VALUE 1
#define GDK_ARRAY_PREALLOC 1024
#define GDK_ARRAY_NO_MEMSET 1
#include "gdk/gdkarrayimpl.c"

#define N_DESCRIPTOR_SETS 2

struct _GskVulkanRender
{
  GskRenderer *renderer;
  GdkVulkanContext *vulkan;

  graphene_rect_t viewport;
  cairo_region_t *clip;

  GskVulkanCommandPool *command_pool;
  VkFence fence;
  VkDescriptorSetLayout descriptor_set_layouts[N_DESCRIPTOR_SETS];
  VkPipelineLayout pipeline_layout;

  GskVulkanRenderOps render_ops;
  GskVulkanOp *first_op;

  GskDescriptorImageInfos descriptor_images;
  GskDescriptorBufferInfos descriptor_buffers;
  VkDescriptorPool descriptor_pool;
  VkDescriptorSet descriptor_sets[N_DESCRIPTOR_SETS];
  GHashTable *pipeline_cache;
  GHashTable *render_pass_cache;

  GskVulkanImage *target;

  GskVulkanBuffer *vertex_buffer;
  VkSampler samplers[3];
  GskVulkanBuffer *storage_buffer;
  guchar *storage_buffer_memory;
  gsize storage_buffer_used;

  GQuark render_pass_counter;
  GQuark gpu_time_timer;
};

typedef struct _PipelineCacheKey PipelineCacheKey;
typedef struct _RenderPassCacheKey RenderPassCacheKey;

struct _PipelineCacheKey
{
  const GskVulkanOpClass *op_class;
  GskVulkanShaderClip clip;
  VkFormat format;
};

struct _RenderPassCacheKey
{
  VkFormat format;
  VkImageLayout from_layout;
  VkImageLayout to_layout;
};

static guint
pipeline_cache_key_hash (gconstpointer data)
{
  const PipelineCacheKey *key = data;

  return GPOINTER_TO_UINT (key->op_class) ^
         key->clip ^
         (key->format << 2);
}

static gboolean
pipeline_cache_key_equal (gconstpointer a,
                          gconstpointer b)
{
  const PipelineCacheKey *keya = a;
  const PipelineCacheKey *keyb = b;

  return keya->op_class == keyb->op_class &&
         keya->clip == keyb->clip &&
         keya->format == keyb->format;
}

static guint
render_pass_cache_key_hash (gconstpointer data)
{
  const RenderPassCacheKey *key = data;

  return (key->from_layout << 20) ^
         (key->to_layout << 16) ^
         (key->format);
}

static gboolean
render_pass_cache_key_equal (gconstpointer a,
                             gconstpointer b)
{
  const RenderPassCacheKey *keya = a;
  const RenderPassCacheKey *keyb = b;

  return keya->from_layout == keyb->from_layout &&
         keya->to_layout == keyb->to_layout &&
         keya->format == keyb->format;
}

static void
gsk_vulkan_render_verbose_print (GskVulkanRender *self,
                                 const char      *heading)
{
#ifdef G_ENABLE_DEBUG
  if (GSK_RENDERER_DEBUG_CHECK (self->renderer, VERBOSE))
    {
      GskVulkanOp *op;
      guint indent = 1;
      GString *string = g_string_new (heading);
      g_string_append (string, ":\n");

      for (op = self->first_op; op; op = op->next)
        {
          if (op->op_class->stage == GSK_VULKAN_STAGE_END_PASS)
            indent--;
          gsk_vulkan_op_print (op, string, indent);
          if (op->op_class->stage == GSK_VULKAN_STAGE_BEGIN_PASS)
            indent++;
        }

      g_print ("%s\n", string->str);
      g_string_free (string, TRUE);
    }
#endif
}

static void
gsk_vulkan_render_setup (GskVulkanRender       *self,
                         GskVulkanImage        *target,
                         const graphene_rect_t *rect,
                         const cairo_region_t  *clip)
{
  self->target = g_object_ref (target);
  self->viewport = *rect;

  if (clip)
    {
      self->clip = cairo_region_reference ((cairo_region_t *) clip);
    }
  else
    {
      self->clip = cairo_region_create_rectangle (&(cairo_rectangle_int_t) {
                                                      0, 0,
                                                      gsk_vulkan_image_get_width (target),
                                                      gsk_vulkan_image_get_height (target)
                                                  });
    }
}

GskVulkanRender *
gsk_vulkan_render_new (GskRenderer      *renderer,
                       GdkVulkanContext *context)
{
  GskVulkanRender *self;
  VkDevice device;

  self = g_new0 (GskVulkanRender, 1);

  self->vulkan = context;
  self->renderer = renderer;
  gsk_descriptor_image_infos_init (&self->descriptor_images);
  gsk_descriptor_buffer_infos_init (&self->descriptor_buffers);

  device = gdk_vulkan_context_get_device (self->vulkan);

  self->command_pool = gsk_vulkan_command_pool_new (self->vulkan);
  GSK_VK_CHECK (vkCreateFence, device,
                               &(VkFenceCreateInfo) {
                                   .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                                   .flags = VK_FENCE_CREATE_SIGNALED_BIT
                               },
                               NULL,
                               &self->fence);

  GSK_VK_CHECK (vkCreateDescriptorPool, device,
                                        &(VkDescriptorPoolCreateInfo) {
                                            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                            .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
                                            .maxSets = N_DESCRIPTOR_SETS,
                                            .poolSizeCount = N_DESCRIPTOR_SETS,
                                            .pPoolSizes = (VkDescriptorPoolSize[N_DESCRIPTOR_SETS]) {
                                                {
                                                    .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                    .descriptorCount = DESCRIPTOR_POOL_MAXITEMS
                                                },
                                                {
                                                    .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                    .descriptorCount = DESCRIPTOR_POOL_MAXITEMS
                                                }
                                            }
                                        },
                                        NULL,
                                        &self->descriptor_pool);

  GSK_VK_CHECK (vkCreateDescriptorSetLayout, device,
                                             &(VkDescriptorSetLayoutCreateInfo) {
                                                 .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                                 .bindingCount = 1,
                                                 .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
                                                 .pBindings = (VkDescriptorSetLayoutBinding[1]) {
                                                     {
                                                         .binding = 0,
                                                         .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                         .descriptorCount = DESCRIPTOR_POOL_MAXITEMS,
                                                         .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
                                                     }
                                                 },
                                                 .pNext = &(VkDescriptorSetLayoutBindingFlagsCreateInfo) {
                                                   .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                                                   .bindingCount = 1,
                                                   .pBindingFlags = (VkDescriptorBindingFlags[1]) {
                                                     VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
                                                     | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT
                                                     | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
                                                   },
                                                 }
                                             },
                                             NULL,
                                             &self->descriptor_set_layouts[0]);

  GSK_VK_CHECK (vkCreateDescriptorSetLayout, device,
                                             &(VkDescriptorSetLayoutCreateInfo) {
                                                 .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                                 .bindingCount = 1,
                                                 .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
                                                 .pBindings = (VkDescriptorSetLayoutBinding[1]) {
                                                     {
                                                         .binding = 0,
                                                         .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                                         .descriptorCount = DESCRIPTOR_POOL_MAXITEMS,
                                                         .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
                                                     },
                                                 },
                                                 .pNext = &(VkDescriptorSetLayoutBindingFlagsCreateInfo) {
                                                   .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                                                   .bindingCount = 1,
                                                   .pBindingFlags = (VkDescriptorBindingFlags[1]) {
                                                     VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT
                                                     | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT
                                                     | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
                                                   },
                                                 }
                                             },
                                             NULL,
                                             &self->descriptor_set_layouts[1]);

  GSK_VK_CHECK (vkCreatePipelineLayout, device,
                                        &(VkPipelineLayoutCreateInfo) {
                                            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                            .setLayoutCount = G_N_ELEMENTS (self->descriptor_set_layouts),
                                            .pSetLayouts = self->descriptor_set_layouts,
                                            .pushConstantRangeCount = gsk_vulkan_push_constants_get_range_count (),
                                            .pPushConstantRanges = gsk_vulkan_push_constants_get_ranges ()
                                        },
                                        NULL,
                                        &self->pipeline_layout);

  GSK_VK_CHECK (vkCreateSampler, device,
                                 &(VkSamplerCreateInfo) {
                                     .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                     .magFilter = VK_FILTER_LINEAR,
                                     .minFilter = VK_FILTER_LINEAR,
                                     .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                     .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                     .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                     .borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
                                     .unnormalizedCoordinates = VK_FALSE,
                                     .maxAnisotropy = 1.0,
                                 },
                                 NULL,
                                 &self->samplers[GSK_VULKAN_SAMPLER_DEFAULT]);

  GSK_VK_CHECK (vkCreateSampler, device,
                                 &(VkSamplerCreateInfo) {
                                     .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                     .magFilter = VK_FILTER_LINEAR,
                                     .minFilter = VK_FILTER_LINEAR,
                                     .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                     .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                     .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                     .borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
                                     .unnormalizedCoordinates = VK_FALSE,
                                     .maxAnisotropy = 1.0,
                                 },
                                 NULL,
                                 &self->samplers[GSK_VULKAN_SAMPLER_REPEAT]);
  
  GSK_VK_CHECK (vkCreateSampler, device,
                                 &(VkSamplerCreateInfo) {
                                     .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                     .magFilter = VK_FILTER_NEAREST,
                                     .minFilter = VK_FILTER_NEAREST,
                                     .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                     .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                     .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                     .borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
                                     .unnormalizedCoordinates = VK_FALSE,
                                     .maxAnisotropy = 1.0,
                                 },
                                 NULL,
                                 &self->samplers[GSK_VULKAN_SAMPLER_NEAREST]);
  

  gsk_vulkan_render_ops_init (&self->render_ops);
  self->pipeline_cache = g_hash_table_new (pipeline_cache_key_hash, pipeline_cache_key_equal);
  self->render_pass_cache = g_hash_table_new (render_pass_cache_key_hash, render_pass_cache_key_equal);

#ifdef G_ENABLE_DEBUG
  self->render_pass_counter = g_quark_from_static_string ("render-passes");
  self->gpu_time_timer = g_quark_from_static_string ("gpu-time");
#endif

  return self;
}

VkFence
gsk_vulkan_render_get_fence (GskVulkanRender *self)
{
  return self->fence;
}

static void
gsk_vulkan_render_seal_ops (GskVulkanRender *self)
{
  GskVulkanOp *last, *op;
  guint i;

  self->first_op = (GskVulkanOp *) gsk_vulkan_render_ops_index (&self->render_ops, 0);

  last = self->first_op;
  for (i = last->op_class->size; i < gsk_vulkan_render_ops_get_size (&self->render_ops); i += op->op_class->size)
    {
      op = (GskVulkanOp *) gsk_vulkan_render_ops_index (&self->render_ops, i);

      last->next = op;
      last = op;
    }
}

typedef struct 
{
  struct {
    GskVulkanOp *first;
    GskVulkanOp *last;
  } upload, command;
} SortData;

static GskVulkanOp *
gsk_vulkan_render_sort_render_pass (GskVulkanRender *self,
                                    GskVulkanOp     *op,
                                    SortData        *sort_data)
{
  while (op)
    {
      switch (op->op_class->stage)
      {
        case GSK_VULKAN_STAGE_UPLOAD:
          if (sort_data->upload.first == NULL)
            sort_data->upload.first = op;
          else
            sort_data->upload.last->next = op;
          sort_data->upload.last = op;
          op = op->next;
          break;

        case GSK_VULKAN_STAGE_COMMAND:
        case GSK_VULKAN_STAGE_SHADER:
          if (sort_data->command.first == NULL)
            sort_data->command.first = op;
          else
            sort_data->command.last->next = op;
          sort_data->command.last = op;
          op = op->next;
          break;

        case GSK_VULKAN_STAGE_BEGIN_PASS:
          {
            SortData pass_data = { { NULL, NULL }, { op, op } };

            op = gsk_vulkan_render_sort_render_pass (self, op->next, &pass_data);

            if (pass_data.upload.first)
              {
                if (sort_data->upload.last == NULL)
                  sort_data->upload.last = pass_data.upload.last;
                else
                  pass_data.upload.last->next = sort_data->upload.first;
                sort_data->upload.first = pass_data.upload.first;
              }
            if (sort_data->command.last == NULL)
              sort_data->command.last = pass_data.command.last;
            else
              pass_data.command.last->next = sort_data->command.first;
            sort_data->command.first = pass_data.command.first;
          }
          break;

        case GSK_VULKAN_STAGE_END_PASS:
          sort_data->command.last->next = op;
          sort_data->command.last = op;
          return op->next;

        default:
          g_assert_not_reached ();
          break;
      }
    }

  return op;
}

static void
gsk_vulkan_render_sort_ops (GskVulkanRender *self)
{
  SortData sort_data = { { NULL, }, };
  
  gsk_vulkan_render_sort_render_pass (self, self->first_op, &sort_data);

  if (sort_data.upload.first)
    {
      sort_data.upload.last->next = sort_data.command.first;
      self->first_op = sort_data.upload.first;
    }
  else
    self->first_op = sort_data.command.first;
  if (sort_data.command.last)
    sort_data.command.last->next = NULL;
}

static void
gsk_vulkan_render_add_node (GskVulkanRender       *self,
                            GskRenderNode         *node,
                            GskVulkanDownloadFunc  download_func,
                            gpointer               download_data)
{
  GskVulkanRenderPass *render_pass;
  cairo_rectangle_int_t extents;

  cairo_region_get_extents (self->clip, &extents);

  gsk_vulkan_render_pass_begin_op (self,
                                   self->target,
                                   &extents,
                                   VK_IMAGE_LAYOUT_UNDEFINED,
                                   VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

  render_pass = gsk_vulkan_render_pass_new ();
  gsk_vulkan_render_pass_add (render_pass,
                              self,
                              gsk_vulkan_image_get_width (self->target),
                              gsk_vulkan_image_get_height (self->target),
                              &extents,
                              node,
                              &self->viewport);
  gsk_vulkan_render_pass_free (render_pass);

  gsk_vulkan_render_pass_end_op (self,
                                 self->target,
                                 VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

  if (download_func)
    gsk_vulkan_download_op (self, self->target, download_func, download_data);

  gsk_vulkan_render_seal_ops (self);
  gsk_vulkan_render_verbose_print (self, "start of frame");
  gsk_vulkan_render_sort_ops (self);
  gsk_vulkan_render_verbose_print (self, "after sort");
}

VkPipelineLayout
gsk_vulkan_render_get_pipeline_layout (GskVulkanRender *self)
{
  return self->pipeline_layout;
}

VkPipeline
gsk_vulkan_render_get_pipeline (GskVulkanRender        *self,
                                const GskVulkanOpClass *op_class,
                                GskVulkanShaderClip     clip,
                                VkRenderPass            render_pass)
{
  const GskVulkanShaderOpClass *shader_op_class = (const GskVulkanShaderOpClass *) op_class;
  static const char *clip_names[] = { "", "-clip", "-clip-rounded" };
  PipelineCacheKey cache_key;
  VkPipeline pipeline;
  GdkDisplay *display;
  char *vertex_shader_name, *fragment_shader_name;

  cache_key = (PipelineCacheKey) {
    .op_class = op_class,
    .clip = clip,
    .format = gsk_vulkan_image_get_vk_format (self->target)
  };
  pipeline = g_hash_table_lookup (self->pipeline_cache, &cache_key);
  if (pipeline)
    return pipeline;

  display = gdk_draw_context_get_display (GDK_DRAW_CONTEXT (self->vulkan));
  vertex_shader_name = g_strconcat ("/org/gtk/libgsk/vulkan/", shader_op_class->shader_name, clip_names[clip], ".vert.spv", NULL);
  fragment_shader_name = g_strconcat ("/org/gtk/libgsk/vulkan/", shader_op_class->shader_name, clip_names[clip], ".frag.spv", NULL);

  GSK_VK_CHECK (vkCreateGraphicsPipelines, gdk_vulkan_context_get_device (self->vulkan),
                                           gdk_vulkan_context_get_pipeline_cache (self->vulkan),
                                           1,
                                           &(VkGraphicsPipelineCreateInfo) {
                                               .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                                               .stageCount = 2,
                                               .pStages = (VkPipelineShaderStageCreateInfo[2]) {
                                                   {
                                                       .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                       .stage = VK_SHADER_STAGE_VERTEX_BIT,
                                                       .module = gdk_display_get_vk_shader_module (display, vertex_shader_name),
                                                       .pName = "main",
                                                   },
                                                   {
                                                       .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                       .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                       .module = gdk_display_get_vk_shader_module (display, fragment_shader_name),
                                                       .pName = "main",
                                                   },
                                               },
                                               .pVertexInputState = shader_op_class->vertex_input_state,
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
                                                   .pAttachments = (VkPipelineColorBlendAttachmentState []) {
                                                       {
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
                                                   }
                                               },
                                               .pDynamicState = &(VkPipelineDynamicStateCreateInfo) {
                                                   .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                                                   .dynamicStateCount = 2,
                                                   .pDynamicStates = (VkDynamicState[2]) {
                                                       VK_DYNAMIC_STATE_VIEWPORT,
                                                       VK_DYNAMIC_STATE_SCISSOR
                                                   },
                                               },
                                               .layout = self->pipeline_layout,
                                               .renderPass = render_pass,
                                               .subpass = 0,
                                               .basePipelineHandle = VK_NULL_HANDLE,
                                               .basePipelineIndex = -1,
                                           },
                                           NULL,
                                           &pipeline);

  g_free (fragment_shader_name);
  g_free (vertex_shader_name);

  g_hash_table_insert (self->pipeline_cache, g_memdup (&cache_key, sizeof (PipelineCacheKey)), pipeline);
  gdk_vulkan_context_pipeline_cache_updated (self->vulkan);

  return pipeline;
}

VkRenderPass
gsk_vulkan_render_get_render_pass (GskVulkanRender *self,
                                   VkFormat         format,
                                   VkImageLayout    from_layout,
                                   VkImageLayout    to_layout)
{
  RenderPassCacheKey cache_key;
  VkRenderPass render_pass;

  cache_key = (RenderPassCacheKey) {
    .format = format,
    .from_layout = from_layout,
    .to_layout = to_layout,
  };
  render_pass = g_hash_table_lookup (self->render_pass_cache, &cache_key);
  if (render_pass)
    return render_pass;

  GSK_VK_CHECK (vkCreateRenderPass, gdk_vulkan_context_get_device (self->vulkan),
                                    &(VkRenderPassCreateInfo) {
                                        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                                        .attachmentCount = 1,
                                        .pAttachments = (VkAttachmentDescription[]) {
                                           {
                                              .format = format,
                                              .samples = VK_SAMPLE_COUNT_1_BIT,
                                              .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                              .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                                              .initialLayout = from_layout,
                                              .finalLayout = to_layout
                                           }
                                        },
                                        .subpassCount = 1,
                                        .pSubpasses = (VkSubpassDescription []) {
                                           {
                                              .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                                              .inputAttachmentCount = 0,
                                              .colorAttachmentCount = 1,
                                              .pColorAttachments = (VkAttachmentReference []) {
                                                 {
                                                    .attachment = 0,
                                                     .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                                                  }
                                               },
                                               .pResolveAttachments = (VkAttachmentReference []) {
                                                  {
                                                     .attachment = VK_ATTACHMENT_UNUSED,
                                                     .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
                                                  }
                                               },
                                               .pDepthStencilAttachment = NULL,
                                            }
                                         },
                                         .dependencyCount = 0,
                                      },
                                      NULL,
                                      &render_pass);

  g_hash_table_insert (self->render_pass_cache, g_memdup (&cache_key, sizeof (RenderPassCacheKey)), render_pass);

  return render_pass;
}

gsize
gsk_vulkan_render_get_image_descriptor (GskVulkanRender        *self,
                                        GskVulkanImage         *image,
                                        GskVulkanRenderSampler  render_sampler)
{
  gsize result;

  result = gsk_descriptor_image_infos_get_size (&self->descriptor_images);
  gsk_descriptor_image_infos_append (&self->descriptor_images,
                                     &(VkDescriptorImageInfo) {
                                       .sampler = self->samplers[render_sampler],
                                       .imageView = gsk_vulkan_image_get_image_view (image),
                                       .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                     });

  g_assert (result < DESCRIPTOR_POOL_MAXITEMS);

  return result;
}

static void
gsk_vulkan_render_ensure_storage_buffer (GskVulkanRender *self)
{
  if (self->storage_buffer_memory != NULL)
    return;

  if (self->storage_buffer == NULL)
    {
      self->storage_buffer = gsk_vulkan_buffer_new_storage (self->vulkan,
                                                            /* random */
                                                            sizeof (float) * 1024 * 1024);
    }

  self->storage_buffer_memory = gsk_vulkan_buffer_map (self->storage_buffer);

  if (gsk_vulkan_render_get_buffer_descriptor (self, self->storage_buffer) != 0)
    {
      g_assert_not_reached ();
    }
}

gsize
gsk_vulkan_render_get_buffer_descriptor (GskVulkanRender *self,
                                         GskVulkanBuffer *buffer)
{
  gsize result;

  gsk_vulkan_render_ensure_storage_buffer (self);

  result = gsk_descriptor_buffer_infos_get_size (&self->descriptor_buffers);
  gsk_descriptor_buffer_infos_append (&self->descriptor_buffers,
                                      &(VkDescriptorBufferInfo) {
                                        .buffer = gsk_vulkan_buffer_get_buffer (buffer),
                                        .offset = 0,
                                        .range = VK_WHOLE_SIZE
                                      });

  g_assert (result < DESCRIPTOR_POOL_MAXITEMS);

  return result;
}

static inline gsize
round_up (gsize number, gsize divisor)
{
  return (number + divisor - 1) / divisor * divisor;
}

guchar *
gsk_vulkan_render_get_buffer_memory (GskVulkanRender *self,
                                     gsize            size,
                                     gsize            alignment,
                                     gsize           *out_offset)
{
  guchar *result;

  g_assert (alignment >= sizeof (float));

  gsk_vulkan_render_ensure_storage_buffer (self);

  self->storage_buffer_used = round_up (self->storage_buffer_used, alignment);
  result = self->storage_buffer_memory + self->storage_buffer_used;
  *out_offset = self->storage_buffer_used / sizeof (float);

  self->storage_buffer_used += size;

  return result;
}

static void
gsk_vulkan_render_prepare_descriptor_sets (GskVulkanRender *self)
{
  VkDevice device;
  VkWriteDescriptorSet descriptor_sets[N_DESCRIPTOR_SETS];
  gsize n_descriptor_sets;
  GskVulkanOp *op;

  device = gdk_vulkan_context_get_device (self->vulkan);

  for (op = self->first_op; op; op = op->next)
    {
      gsk_vulkan_op_reserve_descriptor_sets (op, self);
    }
  
  if (self->storage_buffer_memory)
    {
      gsk_vulkan_buffer_unmap (self->storage_buffer);
      self->storage_buffer_memory = NULL;
      self->storage_buffer_used = 0;
    }

  GSK_VK_CHECK (vkAllocateDescriptorSets, device,
                                          &(VkDescriptorSetAllocateInfo) {
                                              .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                              .descriptorPool = self->descriptor_pool,
                                              .descriptorSetCount = N_DESCRIPTOR_SETS,
                                              .pSetLayouts = self->descriptor_set_layouts,
                                              .pNext = &(VkDescriptorSetVariableDescriptorCountAllocateInfo) {
                                                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
                                                .descriptorSetCount = N_DESCRIPTOR_SETS,
                                                .pDescriptorCounts = (uint32_t[N_DESCRIPTOR_SETS]) {
                                                  MAX (1, gsk_descriptor_image_infos_get_size (&self->descriptor_images)),
                                                  MAX (1, gsk_descriptor_buffer_infos_get_size (&self->descriptor_buffers))
                                                }
                                              }
                                          },
                                          self->descriptor_sets);

  n_descriptor_sets = 0;
  if (gsk_descriptor_image_infos_get_size (&self->descriptor_images) > 0)
    {
      descriptor_sets[n_descriptor_sets++] = (VkWriteDescriptorSet) {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = self->descriptor_sets[0],
          .dstBinding = 0,
          .dstArrayElement = 0,
          .descriptorCount = gsk_descriptor_image_infos_get_size (&self->descriptor_images),
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .pImageInfo = gsk_descriptor_image_infos_get_data (&self->descriptor_images)
      };
    }
  if (gsk_descriptor_buffer_infos_get_size (&self->descriptor_buffers) > 0)
    {
      descriptor_sets[n_descriptor_sets++] = (VkWriteDescriptorSet) {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = self->descriptor_sets[1],
          .dstBinding = 0,
          .dstArrayElement = 0,
          .descriptorCount = gsk_descriptor_buffer_infos_get_size (&self->descriptor_buffers),
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
          .pBufferInfo = gsk_descriptor_buffer_infos_get_data (&self->descriptor_buffers)
      };
    }

  vkUpdateDescriptorSets (device,
                          n_descriptor_sets,
                          descriptor_sets,
                          0, NULL);
}

static void
gsk_vulkan_render_collect_vertex_buffer (GskVulkanRender *self)
{
  GskVulkanOp *op;
  gsize n_bytes;
  guchar *data;

  n_bytes = 0;
  for (op = self->first_op; op; op = op->next)
    {
      n_bytes = gsk_vulkan_op_count_vertex_data (op, n_bytes);
    }
  if (n_bytes == 0)
    return;

  if (self->vertex_buffer && gsk_vulkan_buffer_get_size (self->vertex_buffer) < n_bytes)
    g_clear_pointer (&self->vertex_buffer, gsk_vulkan_buffer_free);

  if (self->vertex_buffer == NULL)
    self->vertex_buffer = gsk_vulkan_buffer_new (self->vulkan, round_up (n_bytes, VERTEX_BUFFER_SIZE_STEP));

  data = gsk_vulkan_buffer_map (self->vertex_buffer);
  for (op = self->first_op; op; op = op->next)
    {
      gsk_vulkan_op_collect_vertex_data (op, data);
    }
  gsk_vulkan_buffer_unmap (self->vertex_buffer);
}

static void
gsk_vulkan_render_submit (GskVulkanRender *self)
{
  VkCommandBuffer command_buffer;
  GskVulkanOp *op;

#ifdef G_ENABLE_DEBUG
  if (GSK_RENDERER_DEBUG_CHECK (self->renderer, SYNC))
    gsk_profiler_timer_begin (gsk_renderer_get_profiler (self->renderer), self->gpu_time_timer);
#endif

  gsk_vulkan_render_prepare_descriptor_sets (self);

  gsk_vulkan_render_collect_vertex_buffer (self);

  command_buffer = gsk_vulkan_command_pool_get_buffer (self->command_pool);

  if (self->vertex_buffer)
    vkCmdBindVertexBuffers (command_buffer,
                            0,
                            1,
                            (VkBuffer[1]) {
                                gsk_vulkan_buffer_get_buffer (self->vertex_buffer)
                            },
                            (VkDeviceSize[1]) { 0 });

  vkCmdBindDescriptorSets (command_buffer,
                           VK_PIPELINE_BIND_POINT_GRAPHICS,
                           self->pipeline_layout,
                           0,
                           N_DESCRIPTOR_SETS,
                           self->descriptor_sets,
                           0,
                           NULL);

  op = self->first_op;
  while (op)
    {
      op = gsk_vulkan_op_command (op, self, VK_NULL_HANDLE, command_buffer);
    }

  gsk_vulkan_command_pool_submit_buffer (self->command_pool,
                                         command_buffer,
                                         0,
                                         NULL,
                                         0,
                                         NULL,
                                         self->fence);

#ifdef G_ENABLE_DEBUG
  if (GSK_RENDERER_DEBUG_CHECK (self->renderer, SYNC))
    {
      GskProfiler *profiler;
      gint64 gpu_time;

      GSK_VK_CHECK (vkWaitForFences, gdk_vulkan_context_get_device (self->vulkan),
                                     1,
                                     &self->fence,
                                     VK_TRUE,
                                     INT64_MAX);

      profiler = gsk_renderer_get_profiler (self->renderer);
      gpu_time = gsk_profiler_timer_end (profiler, self->gpu_time_timer);
      gsk_profiler_timer_set (profiler, self->gpu_time_timer, gpu_time);
    }
#endif
}

static void
gsk_vulkan_render_cleanup (GskVulkanRender *self)
{
  VkDevice device = gdk_vulkan_context_get_device (self->vulkan);
  GskVulkanOp *op;
  gsize i;

  /* XXX: Wait for fence here or just in reset()? */
  GSK_VK_CHECK (vkWaitForFences, device,
                                 1,
                                 &self->fence,
                                 VK_TRUE,
                                 INT64_MAX);

  GSK_VK_CHECK (vkResetFences, device,
                               1,
                               &self->fence);

  for (i = 0; i < gsk_vulkan_render_ops_get_size (&self->render_ops); i += op->op_class->size)
    {
      op = (GskVulkanOp *) gsk_vulkan_render_ops_index (&self->render_ops, i);

      gsk_vulkan_op_finish (op);
    }
  gsk_vulkan_render_ops_set_size (&self->render_ops, 0);

  gsk_vulkan_command_pool_reset (self->command_pool);

  GSK_VK_CHECK (vkResetDescriptorPool, device,
                                       self->descriptor_pool,
                                       0);
  gsk_descriptor_image_infos_set_size (&self->descriptor_images, 0);
  gsk_descriptor_buffer_infos_set_size (&self->descriptor_buffers, 0);

  g_clear_pointer (&self->clip, cairo_region_destroy);
  g_clear_object (&self->target);
}

void
gsk_vulkan_render_free (GskVulkanRender *self)
{
  VkDevice device;
  GHashTableIter iter;
  gpointer key, value;
  guint i;
  
  gsk_vulkan_render_cleanup (self);

  g_clear_pointer (&self->storage_buffer, gsk_vulkan_buffer_free);
  g_clear_pointer (&self->vertex_buffer, gsk_vulkan_buffer_free);

  device = gdk_vulkan_context_get_device (self->vulkan);

  g_hash_table_iter_init (&iter, self->pipeline_cache);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      g_free (key);
      vkDestroyPipeline (device, value, NULL);
    }
  g_hash_table_unref (self->pipeline_cache);

  g_hash_table_iter_init (&iter, self->render_pass_cache);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      g_free (key);
      vkDestroyRenderPass (device, value, NULL);
    }
  g_hash_table_unref (self->render_pass_cache);

  gsk_vulkan_render_ops_clear (&self->render_ops);


  vkDestroyPipelineLayout (device,
                           self->pipeline_layout,
                           NULL);

  vkDestroyDescriptorPool (device,
                           self->descriptor_pool,
                           NULL);
  gsk_descriptor_image_infos_clear (&self->descriptor_images);
  gsk_descriptor_buffer_infos_clear (&self->descriptor_buffers);

  for (i = 0; i < N_DESCRIPTOR_SETS; i++)
    vkDestroyDescriptorSetLayout (device,
                                  self->descriptor_set_layouts[i],
                                  NULL);

  vkDestroyFence (device,
                  self->fence,
                  NULL);

  for (i = 0; i < G_N_ELEMENTS (self->samplers); i++)
    {
      vkDestroySampler (device,
                        self->samplers[i],
                        NULL);
    }

  gsk_vulkan_command_pool_free (self->command_pool);

  g_free (self);
}

gboolean
gsk_vulkan_render_is_busy (GskVulkanRender *self)
{
  return vkGetFenceStatus (gdk_vulkan_context_get_device (self->vulkan), self->fence) != VK_SUCCESS;
}

void
gsk_vulkan_render_render (GskVulkanRender       *self,
                          GskVulkanImage        *target,
                          const graphene_rect_t *rect,
                          const cairo_region_t  *clip,
                          GskRenderNode         *node,
                          GskVulkanDownloadFunc  download_func,
                          gpointer               download_data)
{
  gsk_vulkan_render_cleanup (self);

  gsk_vulkan_render_setup (self, target, rect, clip);

  gsk_vulkan_render_add_node (self, node, download_func, download_data);

  gsk_vulkan_render_submit (self);
}

GskRenderer *
gsk_vulkan_render_get_renderer (GskVulkanRender *self)
{
  return self->renderer;
}

GdkVulkanContext *
gsk_vulkan_render_get_context (GskVulkanRender *self)
{
  return self->vulkan;
}

gpointer
gsk_vulkan_render_alloc_op (GskVulkanRender *self,
                            gsize            size)
{
  gsize pos;

  pos = gsk_vulkan_render_ops_get_size (&self->render_ops);

  gsk_vulkan_render_ops_splice (&self->render_ops,
                                pos,
                                0, FALSE,
                                NULL,
                                size);

  return gsk_vulkan_render_ops_index (&self->render_ops, pos);
}

