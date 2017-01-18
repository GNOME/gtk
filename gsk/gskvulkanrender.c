#include "config.h"

#include "gskvulkanrenderprivate.h"

#include "gskrendererprivate.h"
#include "gskvulkanbufferprivate.h"
#include "gskvulkancommandpoolprivate.h"
#include "gskvulkanpipelineprivate.h"
#include "gskvulkanrenderpassprivate.h"

#include "gskvulkanblendpipelineprivate.h"
#include "gskvulkanborderpipelineprivate.h"
#include "gskvulkanboxshadowpipelineprivate.h"
#include "gskvulkancolorpipelineprivate.h"
#include "gskvulkaneffectpipelineprivate.h"
#include "gskvulkanlineargradientpipelineprivate.h"

#define ORTHO_NEAR_PLANE        -10000
#define ORTHO_FAR_PLANE          10000

#define DESCRIPTOR_POOL_MAXSETS 128
#define DESCRIPTOR_POOL_MAXSETS_INCREASE 128

struct _GskVulkanRender
{
  GskRenderer *renderer;
  GdkVulkanContext *vulkan;

  graphene_matrix_t mvp;
  int scale_factor;
  VkRect2D viewport;
  cairo_region_t *clip;

  GHashTable *framebuffers;
  GskVulkanCommandPool *command_pool;
  VkFence fence;
  VkRenderPass render_pass;
  GskVulkanPipelineLayout *layout;
  GskVulkanUploader *uploader;
  GskVulkanBuffer *vertex_buffer;

  GHashTable *descriptor_set_indexes;
  VkDescriptorPool descriptor_pool;
  uint32_t descriptor_pool_maxsets;
  VkDescriptorSet *descriptor_sets;  
  gsize n_descriptor_sets;
  GskVulkanPipeline *pipelines[GSK_VULKAN_N_PIPELINES];

  GskVulkanImage *target;

  GSList *render_passes;
  GSList *cleanup_images;
};

static void
gsk_vulkan_render_setup (GskVulkanRender       *self,
                         GskVulkanImage        *target,
                         const graphene_rect_t *rect)
{
  GdkWindow *window = gsk_renderer_get_window (self->renderer);
  graphene_matrix_t modelview, projection;

  self->target = g_object_ref (target);

  if (rect)
    {
      self->viewport = (VkRect2D) { { rect->origin.x, rect->origin.y }, { rect->size.width, rect->size.height } };
      self->scale_factor = 1;
      self->clip = cairo_region_create_rectangle (&(cairo_rectangle_int_t) {
                                                      0, 0,
                                                      gsk_vulkan_image_get_width (target), gsk_vulkan_image_get_height (target)
                                                  });
    }
  else
    {
      self->scale_factor = gsk_renderer_get_scale_factor (self->renderer);
      self->viewport.offset = (VkOffset2D) { 0, 0 };
      self->viewport.extent.width = gdk_window_get_width (window) * self->scale_factor;
      self->viewport.extent.height = gdk_window_get_height (window) * self->scale_factor;
      self->clip = gdk_drawing_context_get_clip (gsk_renderer_get_drawing_context (self->renderer));
    }

  graphene_matrix_init_scale (&modelview, self->scale_factor, self->scale_factor, 1.0);
  graphene_matrix_init_ortho (&projection,
                              self->viewport.offset.x, self->viewport.offset.x + self->viewport.extent.width,
                              self->viewport.offset.y, self->viewport.offset.y + self->viewport.extent.height,
                              ORTHO_NEAR_PLANE,
                              ORTHO_FAR_PLANE);

  graphene_matrix_multiply (&modelview, &projection, &self->mvp);
}

GskVulkanRender *
gsk_vulkan_render_new (GskRenderer      *renderer,
                       GdkVulkanContext *context)
{
  GskVulkanRender *self;
  VkDevice device;

  self = g_slice_new0 (GskVulkanRender);

  self->vulkan = context;
  self->renderer = renderer;
  self->framebuffers = g_hash_table_new (g_direct_hash, g_direct_equal);
  self->descriptor_set_indexes = g_hash_table_new (g_direct_hash, g_direct_equal);

  device = gdk_vulkan_context_get_device (self->vulkan);

  self->command_pool = gsk_vulkan_command_pool_new (self->vulkan);
  GSK_VK_CHECK (vkCreateFence, device,
                               &(VkFenceCreateInfo) {
                                   .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                                   .flags = VK_FENCE_CREATE_SIGNALED_BIT
                               },
                               NULL,
                               &self->fence);

  self->descriptor_pool_maxsets = DESCRIPTOR_POOL_MAXSETS;
  GSK_VK_CHECK (vkCreateDescriptorPool, device,
                                        &(VkDescriptorPoolCreateInfo) {
                                            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                            .maxSets = self->descriptor_pool_maxsets,
                                            .poolSizeCount = 1,
                                            .pPoolSizes = (VkDescriptorPoolSize[1]) {
                                                {
                                                    .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                    .descriptorCount = self->descriptor_pool_maxsets
                                                }
                                            }
                                        },
                                        NULL,
                                        &self->descriptor_pool);

  GSK_VK_CHECK (vkCreateRenderPass, gdk_vulkan_context_get_device (self->vulkan),
                                    &(VkRenderPassCreateInfo) {
                                        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                                        .attachmentCount = 1,
                                        .pAttachments = (VkAttachmentDescription[]) {
                                           {
                                              .format = gdk_vulkan_context_get_image_format (self->vulkan),
                                              .samples = VK_SAMPLE_COUNT_1_BIT,
                                              .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                              .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                                              .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                              .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
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
                                               .preserveAttachmentCount = 1,
                                               .pPreserveAttachments = (uint32_t []) { 0 },
                                            }
                                         },
                                         .dependencyCount = 0
                                      },
                                      NULL,
                                      &self->render_pass);

  self->layout = gsk_vulkan_pipeline_layout_new (self->vulkan);

  self->uploader = gsk_vulkan_uploader_new (self->vulkan, self->command_pool);

  return self;
}

typedef struct {
  VkFramebuffer framebuffer;
} HashFramebufferEntry;

static void
gsk_vulkan_render_remove_framebuffer_from_image (gpointer  data,
                                                 GObject  *image)
{
  GskVulkanRender *self = data;
  HashFramebufferEntry *fb;

  fb = g_hash_table_lookup (self->framebuffers, image);
  g_hash_table_remove (self->framebuffers, image);

  vkDestroyFramebuffer (gdk_vulkan_context_get_device (self->vulkan),
                        fb->framebuffer,
                        NULL);

  g_slice_free (HashFramebufferEntry, fb);
}

static VkFramebuffer
gsk_vulkan_render_get_framebuffer (GskVulkanRender *self,
                                   GskVulkanImage  *image)
{
  HashFramebufferEntry *fb;

  fb = g_hash_table_lookup (self->framebuffers, image);
  if (fb)
    return fb->framebuffer;

  fb = g_slice_new0 (HashFramebufferEntry);
  GSK_VK_CHECK (vkCreateFramebuffer, gdk_vulkan_context_get_device (self->vulkan),
                                     &(VkFramebufferCreateInfo) {
                                         .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                                         .renderPass = self->render_pass,
                                         .attachmentCount = 1,
                                         .pAttachments = (VkImageView[1]) {
                                             gsk_vulkan_image_get_image_view (image)
                                         },
                                         .width = gsk_vulkan_image_get_width (image),
                                         .height = gsk_vulkan_image_get_height (image),
                                         .layers = 1
                                     },
                                     NULL,
                                     &fb->framebuffer);
  g_hash_table_insert (self->framebuffers, image, fb);
  g_object_weak_ref (G_OBJECT (image), gsk_vulkan_render_remove_framebuffer_from_image, self);

  return fb->framebuffer;
}

void
gsk_vulkan_render_add_cleanup_image (GskVulkanRender *self,
                                     GskVulkanImage  *image)
{
  self->cleanup_images = g_slist_prepend (self->cleanup_images, image);
}

void
gsk_vulkan_render_add_node (GskVulkanRender *self,
                            GskRenderNode   *node)
{
  GskVulkanRenderPass *pass = gsk_vulkan_render_pass_new (self->vulkan);

  self->render_passes = g_slist_prepend (self->render_passes, pass);

  gsk_vulkan_render_pass_add (pass,
                              self,
                              &self->mvp,
                              &GRAPHENE_RECT_INIT (
                                  self->viewport.offset.x, self->viewport.offset.y,
                                  self->viewport.extent.width, self->viewport.extent.height
                              ),
                              node);
}

void
gsk_vulkan_render_upload (GskVulkanRender *self)
{
  GSList *l;

  for (l = self->render_passes; l; l = l->next)
    {
      gsk_vulkan_render_pass_upload (l->data, self, self->uploader);
    }

  gsk_vulkan_uploader_upload (self->uploader);
}

static gsize
gsk_vulkan_renderer_count_vertex_data (GskVulkanRender *self)
{
  gsize n_bytes;
  GSList *l;

  n_bytes = 0;
  for (l = self->render_passes; l; l = l->next)
    {
      n_bytes += gsk_vulkan_render_pass_count_vertex_data (l->data);
    }

  return n_bytes;
}

static GskVulkanBuffer *
gsk_vulkan_render_collect_vertex_data (GskVulkanRender *self)
{
  GskVulkanBuffer *buffer;
  guchar *data;
  GSList *l;
  gsize offset, n_bytes;
  
  offset = 0;
  n_bytes = gsk_vulkan_renderer_count_vertex_data (self);
  buffer = gsk_vulkan_buffer_new (self->vulkan, n_bytes);
  data = gsk_vulkan_buffer_map (buffer);

  for (l = self->render_passes; l; l = l->next)
    {
      offset += gsk_vulkan_render_pass_collect_vertex_data (l->data, data, offset, n_bytes - offset);
      g_assert (offset <= n_bytes);
    }

  gsk_vulkan_buffer_unmap (buffer);

  return buffer;
}

GskVulkanPipeline *
gsk_vulkan_render_get_pipeline (GskVulkanRender       *self,
                                GskVulkanPipelineType  type)
{
  static const struct {
    const char *name;
    GskVulkanPipeline * (* create_func) (GskVulkanPipelineLayout *layout, const char *name, VkRenderPass render_pass);
  } pipeline_info[GSK_VULKAN_N_PIPELINES] = {
    { "blend", gsk_vulkan_blend_pipeline_new },
    { "blend-clip", gsk_vulkan_blend_pipeline_new },
    { "blend-clip-rounded", gsk_vulkan_blend_pipeline_new },
    { "color", gsk_vulkan_color_pipeline_new },
    { "color-clip", gsk_vulkan_color_pipeline_new },
    { "color-clip-rounded", gsk_vulkan_color_pipeline_new },
    { "linear", gsk_vulkan_linear_gradient_pipeline_new },
    { "linear-clip", gsk_vulkan_linear_gradient_pipeline_new },
    { "linear-clip-rounded", gsk_vulkan_linear_gradient_pipeline_new },
    { "color-matrix", gsk_vulkan_effect_pipeline_new },
    { "color-matrix-clip", gsk_vulkan_effect_pipeline_new },
    { "color-matrix-clip-rounded", gsk_vulkan_effect_pipeline_new },
    { "border", gsk_vulkan_border_pipeline_new },
    { "border-clip", gsk_vulkan_border_pipeline_new },
    { "border-clip-rounded", gsk_vulkan_border_pipeline_new },
    { "inset-shadow", gsk_vulkan_box_shadow_pipeline_new },
    { "inset-shadow-clip", gsk_vulkan_box_shadow_pipeline_new },
    { "inset-shadow-clip-rounded", gsk_vulkan_box_shadow_pipeline_new },
    { "outset-shadow", gsk_vulkan_box_shadow_pipeline_new },
    { "outset-shadow-clip", gsk_vulkan_box_shadow_pipeline_new },
    { "outset-shadow-clip-rounded", gsk_vulkan_box_shadow_pipeline_new },
  };

  g_return_val_if_fail (type < GSK_VULKAN_N_PIPELINES, NULL);

  if (self->pipelines[type] == NULL)
    {
      self->pipelines[type] = pipeline_info[type].create_func (self->layout,
                                                               pipeline_info[type].name,
                                                               self->render_pass);
    }

  return self->pipelines[type];
}

VkDescriptorSet
gsk_vulkan_render_get_descriptor_set (GskVulkanRender *self,
                                      gsize            id)
{
  g_assert (id < self->n_descriptor_sets);

  return self->descriptor_sets[id];
}

gsize
gsk_vulkan_render_reserve_descriptor_set (GskVulkanRender *self,
                                          GskVulkanImage  *source)
{
  gpointer id_plus_one;

  id_plus_one = g_hash_table_lookup (self->descriptor_set_indexes, source);
  if (id_plus_one)
    return GPOINTER_TO_SIZE (id_plus_one) - 1;

  id_plus_one = GSIZE_TO_POINTER (g_hash_table_size (self->descriptor_set_indexes) + 1);
  g_hash_table_insert (self->descriptor_set_indexes, source, id_plus_one);
  
  return GPOINTER_TO_SIZE (id_plus_one) - 1;
}

static void
gsk_vulkan_render_prepare_descriptor_sets (GskVulkanRender *self,
                                           VkSampler        sampler)
{
  GHashTableIter iter;
  gpointer key, value;
  VkDevice device;
  GSList *l;
  guint i, needed_sets;

  device = gdk_vulkan_context_get_device (self->vulkan);

  for (l = self->render_passes; l; l = l->next)
    {
      gsk_vulkan_render_pass_reserve_descriptor_sets (l->data, self);
    }
  
  needed_sets = g_hash_table_size (self->descriptor_set_indexes);
  if (needed_sets > self->n_descriptor_sets)
    {
      if (needed_sets > self->descriptor_pool_maxsets)
        {
          guint added_sets = needed_sets - self->descriptor_pool_maxsets;
          added_sets = added_sets + DESCRIPTOR_POOL_MAXSETS_INCREASE - 1;
          added_sets -= added_sets % DESCRIPTOR_POOL_MAXSETS_INCREASE;

          vkDestroyDescriptorPool (device,
                                   self->descriptor_pool,
                                   NULL);
          self->descriptor_pool_maxsets += added_sets;
          GSK_VK_CHECK (vkCreateDescriptorPool, device,
                                                &(VkDescriptorPoolCreateInfo) {
                                                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                                    .maxSets = self->descriptor_pool_maxsets,
                                                    .poolSizeCount = 1,
                                                    .pPoolSizes = (VkDescriptorPoolSize[1]) {
                                                        {
                                                            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                            .descriptorCount = self->descriptor_pool_maxsets
                                                        }
                                                    }
                                                },
                                                NULL,
                                                &self->descriptor_pool);
        }
      else
        {
          GSK_VK_CHECK (vkResetDescriptorPool, device,
                                               self->descriptor_pool,
                                               0);
        }

      self->n_descriptor_sets = needed_sets;
      self->descriptor_sets = g_renew (VkDescriptorSet, self->descriptor_sets, needed_sets);
    }

  VkDescriptorSetLayout *layouts = g_newa (VkDescriptorSetLayout, needed_sets);
  for (i = 0; i < needed_sets; i++)
    {
      layouts[i] = gsk_vulkan_pipeline_layout_get_descriptor_set_layout (self->layout);
    }

  GSK_VK_CHECK (vkAllocateDescriptorSets, device,
                                          &(VkDescriptorSetAllocateInfo) {
                                              .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                              .descriptorPool = self->descriptor_pool,
                                              .descriptorSetCount = needed_sets,
                                              .pSetLayouts = layouts
                                          },
                                          self->descriptor_sets);

  g_hash_table_iter_init (&iter, self->descriptor_set_indexes);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      GskVulkanImage *image = key;
      gsize id = GPOINTER_TO_SIZE (value) - 1;

      vkUpdateDescriptorSets (device,
                              1,
                              (VkWriteDescriptorSet[1]) {
                                  {
                                      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                      .dstSet = self->descriptor_sets[id],
                                      .dstBinding = 0,
                                      .dstArrayElement = 0,
                                      .descriptorCount = 1,
                                      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                      .pImageInfo = &(VkDescriptorImageInfo) {
                                          .sampler = sampler,
                                          .imageView = gsk_vulkan_image_get_image_view (image),
                                          .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                      }
                                  }
                              },
                              0, NULL);
    }
}

void
gsk_vulkan_render_draw (GskVulkanRender   *self,
                        VkSampler          sampler)
{
  VkCommandBuffer command_buffer;
  GSList *l;
  guint i;

  gsk_vulkan_render_prepare_descriptor_sets (self, sampler);

  command_buffer = gsk_vulkan_command_pool_get_buffer (self->command_pool);

  self->vertex_buffer = gsk_vulkan_render_collect_vertex_data (self);

  vkCmdSetViewport (command_buffer,
                    0,
                    1,
                    &(VkViewport) {
                      .x = 0,
                      .y = 0,
                      .width = self->viewport.extent.width,
                      .height = self->viewport.extent.height,
                      .minDepth = 0,
                      .maxDepth = 1
                    });

  for (i = 0; i < cairo_region_num_rectangles (self->clip); i++)
    {
      cairo_rectangle_int_t rect;

      cairo_region_get_rectangle (self->clip, i, &rect);

      vkCmdSetScissor (command_buffer,
                       0,
                       1,
                       &(VkRect2D) {
                           { rect.x * self->scale_factor, rect.y * self->scale_factor },
                           { rect.width * self->scale_factor, rect.height * self->scale_factor }
                       });

      vkCmdBeginRenderPass (command_buffer,
                            &(VkRenderPassBeginInfo) {
                                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                .renderPass = self->render_pass,
                                .framebuffer = gsk_vulkan_render_get_framebuffer (self, self->target),
                                .renderArea = { 
                                    { rect.x * self->scale_factor, rect.y * self->scale_factor },
                                    { rect.width * self->scale_factor, rect.height * self->scale_factor }
                                },
                                .clearValueCount = 1,
                                .pClearValues = (VkClearValue [1]) {
                                    { .color = { .float32 = { 0.f, 0.f, 0.f, 0.f } } }
                                }
                            },
                            VK_SUBPASS_CONTENTS_INLINE);

      for (l = self->render_passes; l; l = l->next)
        {
          gsk_vulkan_render_pass_draw (l->data, self, self->vertex_buffer, self->layout, command_buffer);
        }

      vkCmdEndRenderPass (command_buffer);
    }

  gsk_vulkan_command_pool_submit_buffer (self->command_pool, command_buffer, self->fence);

  if (GSK_RENDER_MODE_CHECK (SYNC))
    {
      GSK_VK_CHECK (vkWaitForFences, gdk_vulkan_context_get_device (self->vulkan),
                                     1,
                                     &self->fence,
                                     VK_TRUE,
                                     INT64_MAX);
    }
}

GskTexture *
gsk_vulkan_render_download_target (GskVulkanRender *self)
{
  gsk_vulkan_uploader_reset (self->uploader);

  return gsk_vulkan_image_download (self->target, self->uploader);
}

static void
gsk_vulkan_render_cleanup (GskVulkanRender *self)
{
  VkDevice device = gdk_vulkan_context_get_device (self->vulkan);

  /* XXX: Wait for fence here or just in reset()? */
  GSK_VK_CHECK (vkWaitForFences, device,
                                 1,
                                 &self->fence,
                                 VK_TRUE,
                                 INT64_MAX);

  GSK_VK_CHECK (vkResetFences, device,
                               1,
                               &self->fence);

  gsk_vulkan_uploader_reset (self->uploader);

  gsk_vulkan_command_pool_reset (self->command_pool);

  g_clear_pointer (&self->vertex_buffer, gsk_vulkan_buffer_free);

  g_hash_table_remove_all (self->descriptor_set_indexes);
  GSK_VK_CHECK (vkResetDescriptorPool, device,
                                       self->descriptor_pool,
                                       0);

  g_slist_free_full (self->render_passes, (GDestroyNotify) gsk_vulkan_render_pass_free);
  self->render_passes = NULL;
  g_slist_free_full (self->cleanup_images, g_object_unref);
  self->cleanup_images = NULL;

  g_clear_pointer (&self->clip, cairo_region_destroy);
  g_clear_object (&self->target);
}

void
gsk_vulkan_render_free (GskVulkanRender *self)
{
  GHashTableIter iter;
  gpointer key, value;
  VkDevice device;
  guint i;
  
  gsk_vulkan_render_cleanup (self);

  device = gdk_vulkan_context_get_device (self->vulkan);

  g_hash_table_iter_init (&iter, self->framebuffers);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      HashFramebufferEntry *fb = value;

      vkDestroyFramebuffer (gdk_vulkan_context_get_device (self->vulkan),
                            fb->framebuffer,
                            NULL);
      g_slice_free (HashFramebufferEntry, fb);
      g_object_weak_unref (G_OBJECT (key), gsk_vulkan_render_remove_framebuffer_from_image, self);
      g_hash_table_iter_remove (&iter);
    }
  g_hash_table_unref (self->framebuffers);

  for (i = 0; i < GSK_VULKAN_N_PIPELINES; i++)
    g_clear_object (&self->pipelines[i]);

  g_clear_pointer (&self->uploader, gsk_vulkan_uploader_free);

  g_clear_pointer (&self->layout, gsk_vulkan_pipeline_layout_unref);

  vkDestroyRenderPass (device,
                       self->render_pass,
                       NULL);

  vkDestroyDescriptorPool (device,
                           self->descriptor_pool,
                           NULL);
  g_free (self->descriptor_sets);
  g_hash_table_unref (self->descriptor_set_indexes);

  vkDestroyFence (device,
                  self->fence,
                  NULL);

  gsk_vulkan_command_pool_free (self->command_pool);

  g_slice_free (GskVulkanRender, self);
}

gboolean
gsk_vulkan_render_is_busy (GskVulkanRender *self)
{
  return vkGetFenceStatus (gdk_vulkan_context_get_device (self->vulkan), self->fence) != VK_SUCCESS;
}

void
gsk_vulkan_render_reset (GskVulkanRender       *self,
                         GskVulkanImage        *target,
                         const graphene_rect_t *rect)
{
  gsk_vulkan_render_cleanup (self);

  gsk_vulkan_render_setup (self, target, rect);
}

GskRenderer *
gsk_vulkan_render_get_renderer (GskVulkanRender *self)
{
  return self->renderer;
}
