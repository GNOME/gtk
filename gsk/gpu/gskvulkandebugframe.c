#include "config.h"

#include "gskvulkandebugframeprivate.h"

#include "gskgpuopprivate.h"
#include "gskdebugnodeprivate.h"
#include "gskrendernodeprivate.h"
#include "gskrenderreplay.h"

#include "gtk/inspector/window.h"

typedef struct _GskVulkanDebugEntry GskVulkanDebugEntry;

/* for position tracking, we can't use 0 because that's a valid index */
#define NO_ITEM G_MAXSIZE

struct _GskVulkanDebugEntry {
  GskRenderNode *node;
  guint pos;
  /* positions in the debug array or NO_ITEM if none */
  gsize parent;
  gsize first_child;
  GskDebugProfile profile;
};

#define GDK_ARRAY_NAME gsk_vulkan_debug
#define GDK_ARRAY_TYPE_NAME GskVulkanDebug
#define GDK_ARRAY_ELEMENT_TYPE GskVulkanDebugEntry
#define GDK_ARRAY_BY_VALUE 1
#include "gdk/gdkarrayimpl.c"

struct _GskVulkanDebugFrame
{
  GskVulkanFrame parent_instance;

  GskRenderNode *node;

  gsize n_ops;
  GskVulkanDebug debug;
  gsize debug_current;

  float vk_timestamp_scale;
  gsize pool_size;

  VkQueryPool vk_timestamp_pool;
  uint64_t *timestamp_pool_values;
  gsize *timestamp_pool_nodes;
  VkQueryPool vk_pixels_pool;
  uint64_t *pixels_pool_values;
};

struct _GskVulkanDebugFrameClass
{
  GskVulkanFrameClass parent_class;
};

G_DEFINE_TYPE (GskVulkanDebugFrame, gsk_vulkan_debug_frame, GSK_TYPE_VULKAN_FRAME)

static void
gsk_vulkan_debug_frame_submit_ops (GskVulkanFrame        *frame,
                                   GskVulkanCommandState *state,
                                   GskGpuOp              *op)
{
  GskVulkanDebugFrame *self = GSK_VULKAN_DEBUG_FRAME (frame);

  if (self->n_ops > self->pool_size)
    {
      GskVulkanDevice *device;
      VkDevice vk_device;

      device = GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (GSK_GPU_FRAME (self)));
      vk_device = gsk_vulkan_device_get_vk_device (device);

      g_free (self->timestamp_pool_values);
      g_free (self->timestamp_pool_nodes);
      vkDestroyQueryPool (vk_device,
                          self->vk_timestamp_pool,
                          NULL);
      /* reserve 50% more than needed */
      self->pool_size = 3 * self->n_ops / 2;

      self->timestamp_pool_values = g_new (uint64_t, self->pool_size * 2);
      self->timestamp_pool_nodes = g_new (uint64_t, self->pool_size);
      GSK_VK_CHECK (vkCreateQueryPool, vk_device,
                                       &(VkQueryPoolCreateInfo) {
                                           .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
                                           .flags = 0,
                                           VK_QUERY_TYPE_TIMESTAMP,
                                           .queryCount = self->pool_size * 2,
                                       },
                                       NULL,
                                       &self->vk_timestamp_pool);

      self->pixels_pool_values = g_new (uint64_t, self->pool_size);
      GSK_VK_CHECK (vkCreateQueryPool, vk_device,
                                       &(VkQueryPoolCreateInfo) {
                                           .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
                                           .flags = 0,
                                           VK_QUERY_TYPE_PIPELINE_STATISTICS,
                                           .queryCount = self->pool_size,
                                           .pipelineStatistics = VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT,
                                       },
                                       NULL,
                                       &self->vk_pixels_pool);
    }

  vkCmdResetQueryPool (state->vk_command_buffer, self->vk_timestamp_pool, 0, self->n_ops * 2);
  self->n_ops = 0;

  while (op)
    {
      if (op->node_id == NO_ITEM)
        {
          op = gsk_gpu_op_vk_command (op, GSK_GPU_FRAME (frame), state);
        }
      else
        {
          GskVulkanDebugEntry *entry;

          self->timestamp_pool_nodes[self->n_ops] = op->node_id;
          vkCmdBeginQuery (state->vk_command_buffer,
                           self->vk_pixels_pool,
                           self->n_ops,
                           0);
          vkCmdWriteTimestamp (state->vk_command_buffer,
                               VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                               self->vk_timestamp_pool,
                               self->n_ops * 2);
          entry = gsk_vulkan_debug_get (&self->debug, op->node_id);
          entry->profile.self.cpu_submit_ns -= g_get_monotonic_time () * 1000;

          op = gsk_gpu_op_vk_command (op, GSK_GPU_FRAME (frame), state);

          entry->profile.self.cpu_submit_ns += g_get_monotonic_time () * 1000;

          vkCmdWriteTimestamp (state->vk_command_buffer,
                               VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                               self->vk_timestamp_pool,
                               self->n_ops * 2 + 1);
          vkCmdEndQuery (state->vk_command_buffer,
                         self->vk_pixels_pool,
                         self->n_ops);
          self->n_ops++;
        }
    }
}

static void
gsk_vulkan_debug_frame_setup (GskGpuFrame *frame)
{
  GskVulkanDebugFrame *self = GSK_VULKAN_DEBUG_FRAME (frame);
  GskVulkanDevice *device;
  VkPhysicalDeviceProperties vk_props;

  GSK_GPU_FRAME_CLASS (gsk_vulkan_debug_frame_parent_class)->setup (frame);

  device = GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (frame));

  vkGetPhysicalDeviceProperties (gsk_vulkan_device_get_vk_physical_device (device), &vk_props);
  self->vk_timestamp_scale = vk_props.limits.timestampPeriod;
}

static GskRenderNode *
gsk_vulkan_debug_frame_filter_node (GskRenderReplay *replay,
                                    GskRenderNode   *node,
                                    gpointer         user_data)
{
  GskVulkanDebugFrame *self = user_data;
  GskVulkanDebugEntry *entry;
  GskRenderNode *result, *child;
  gsize pos;

  pos = self->debug_current;
  /* node wasn't rendered */
  if (pos == NO_ITEM)
    return gsk_render_replay_default (replay, node);

  entry = gsk_vulkan_debug_get (&self->debug, pos);

  self->debug_current = entry->first_child;
  child = gsk_render_replay_default (replay, node);

  entry->profile.self.cpu_record_ns = entry->profile.total.cpu_record_ns;
  entry->profile.total.cpu_submit_ns = entry->profile.self.cpu_submit_ns;
  entry->profile.total.gpu_ns = entry->profile.self.gpu_ns;
  entry->profile.total.gpu_pixels = entry->profile.self.gpu_pixels;
  entry->profile.total.offscreen_pixels = entry->profile.self.offscreen_pixels;
  if (entry->first_child != NO_ITEM)
    {
      gsize i, n_children;

      gsk_render_node_get_children (entry->node, &n_children);
      for (i = 0; i < n_children; i++)
        {
          GskVulkanDebugEntry *child_entry = gsk_vulkan_debug_get (&self->debug, entry->first_child + i);
          entry->profile.self.cpu_record_ns -= child_entry->profile.total.cpu_record_ns;
          entry->profile.total.cpu_submit_ns += child_entry->profile.total.cpu_submit_ns;
          entry->profile.total.gpu_ns += child_entry->profile.total.gpu_ns;
          entry->profile.total.gpu_pixels += child_entry->profile.total.gpu_pixels;
          entry->profile.total.offscreen_pixels += child_entry->profile.total.offscreen_pixels;
        }
    }
  entry->profile.self.cpu_ns = entry->profile.self.cpu_record_ns + entry->profile.self.cpu_submit_ns;
  entry->profile.total.cpu_ns = entry->profile.total.cpu_record_ns + entry->profile.total.cpu_submit_ns;

  result = gsk_debug_node_new_profile (child,
                                       &entry->profile,
                                       g_strdup_printf ("record total   : %lluns\n"
                                                        "record self    : %lluns\n"
                                                        "submit total   : %lluns\n"
                                                        "submit self    : %lluns\n"
                                                        "GPU total      : %lluns\n"
                                                        "GPU self       : %lluns\n"
                                                        "pixels total   : %llu\n"
                                                        "pixels self    : %llu\n"
                                                        "offscreen total: %llu\n"
                                                        "offscreen self : %llu",
                                                        (long long unsigned) entry->profile.total.cpu_record_ns,
                                                        (long long unsigned) entry->profile.self.cpu_record_ns,
                                                        (long long unsigned) entry->profile.total.cpu_submit_ns,
                                                        (long long unsigned) entry->profile.self.cpu_submit_ns,
                                                        (long long unsigned) entry->profile.total.gpu_ns,
                                                        (long long unsigned) entry->profile.self.gpu_ns,
                                                        (long long unsigned) entry->profile.total.gpu_pixels,
                                                        (long long unsigned) entry->profile.self.gpu_pixels,
                                                        (long long unsigned) entry->profile.total.offscreen_pixels,
                                                        (long long unsigned) entry->profile.self.offscreen_pixels));
  gsk_render_node_unref (child);

  self->debug_current = pos + 1;

  return result;
}

static void
gsk_vulkan_debug_frame_process (GskVulkanDebugFrame *self)
{
  GskRenderReplay *replay;
  GskRenderNode *result;

  g_assert (self->debug_current == NO_ITEM);

  self->debug_current = 0;
  replay = gsk_render_replay_new ();
  gsk_render_replay_set_node_filter (replay,
                                     gsk_vulkan_debug_frame_filter_node,
                                     self,
                                     NULL);
  result = gsk_render_replay_filter_node (replay, self->node);
  gsk_render_replay_free (replay);

  gtk_inspector_add_profile_node (gsk_gpu_device_get_display (gsk_gpu_frame_get_device (GSK_GPU_FRAME (self))),
                                  self->node,
                                  result);

  gsk_render_node_unref (result);
  self->debug_current = NO_ITEM;
}

static void
gsk_vulkan_debug_frame_cleanup (GskGpuFrame *frame)
{
  GskVulkanDebugFrame *self = GSK_VULKAN_DEBUG_FRAME (frame);
  GskVulkanDevice *device;
  VkDevice vk_device;
  gsize i;

  device = GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (frame));
  vk_device = gsk_vulkan_device_get_vk_device (device);

  GSK_VK_CHECK (vkGetQueryPoolResults, vk_device,
                                       self->vk_timestamp_pool,
                                       0,
                                       2 * self->n_ops,
                                       2 * self->n_ops * sizeof (uint64_t),
                                       self->timestamp_pool_values,
                                       sizeof (uint64_t),
                                       VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
  GSK_VK_CHECK (vkGetQueryPoolResults, vk_device,
                                       self->vk_pixels_pool,
                                       0,
                                       self->n_ops,
                                       self->n_ops * sizeof (uint64_t),
                                       self->pixels_pool_values,
                                       sizeof (uint64_t),
                                       VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
  for (i = 0; i < self->n_ops; i++)
    {
      GskVulkanDebugEntry *entry;

      g_assert (self->timestamp_pool_nodes[i] < gsk_vulkan_debug_get_size (&self->debug));
      entry = gsk_vulkan_debug_get (&self->debug, self->timestamp_pool_nodes[i]);
      entry->profile.self.gpu_ns += (self->timestamp_pool_values[2 * i + 1] - self->timestamp_pool_values[2 * i])
                                 * self->vk_timestamp_scale;
      entry->profile.self.gpu_pixels += self->pixels_pool_values[i];
    }

  if (self->node)
    gsk_vulkan_debug_frame_process (self);

  gsk_vulkan_debug_clear (&self->debug);
  g_clear_pointer (&self->node, gsk_render_node_unref);

  self->n_ops = 0;
  g_assert (self->debug_current == NO_ITEM);

  GSK_GPU_FRAME_CLASS (gsk_vulkan_debug_frame_parent_class)->cleanup (frame);
}

static gpointer
gsk_vulkan_debug_frame_alloc_op (GskGpuFrame         *frame,
                                 const GskGpuOpClass *op_class)
{
  GskVulkanDebugFrame *self = GSK_VULKAN_DEBUG_FRAME (frame);
  GskGpuOp *op;

  self->n_ops++;

  op = GSK_GPU_FRAME_CLASS (gsk_vulkan_debug_frame_parent_class)->alloc_op (frame, op_class);
  op->node_id = self->debug_current;

  return op;
}

static void
gsk_vulkan_debug_frame_start_node (GskGpuFrame   *frame,
                                   GskRenderNode *node,
                                   gsize          pos)
{
  GskVulkanDebugFrame *self = GSK_VULKAN_DEBUG_FRAME (frame);
  GskVulkanDebugEntry *entry;

  GSK_GPU_FRAME_CLASS (gsk_vulkan_debug_frame_parent_class)->start_node (frame, node, pos);

  if (self->debug_current == NO_ITEM)
    {
      if (gsk_vulkan_debug_get_size (&self->debug) == 0)
        {
          gsk_vulkan_debug_append (&self->debug,
                                   &(GskVulkanDebugEntry) {
                                     .node = node,
                                     .pos = pos,
                                     .parent = NO_ITEM,
                                     .first_child = NO_ITEM
                                   });
          self->node = gsk_render_node_ref (node);
        }
      self->debug_current = 0;
    }
  else
    {
      GskVulkanDebugEntry *cur;
      
      cur = gsk_vulkan_debug_get (&self->debug, self->debug_current);
      if (cur->first_child == NO_ITEM)
        {
          GskRenderNode **children;
          gsize i, n_children;

          children = gsk_render_node_get_children (cur->node, &n_children);
          g_assert (n_children > 0);
          cur->first_child = gsk_vulkan_debug_get_size (&self->debug);

          for (i = 0; i < n_children; i++)
            {
              gsk_vulkan_debug_append (&self->debug,
                                       &(GskVulkanDebugEntry) {
                                         .node = children[i],
                                         .pos = i,
                                         .parent = self->debug_current,
                                         .first_child = NO_ITEM,
                                       });
            }
        }

      self->debug_current = cur->first_child + pos;
    }

  entry = gsk_vulkan_debug_get (&self->debug, self->debug_current);

  entry->profile.total.cpu_record_ns -= g_get_monotonic_time () * 1000;
}

static void
gsk_vulkan_debug_frame_end_node (GskGpuFrame *frame)
{
  GskVulkanDebugFrame *self = GSK_VULKAN_DEBUG_FRAME (frame);
  GskVulkanDebugEntry *entry;

  entry = gsk_vulkan_debug_get (&self->debug, self->debug_current);

  entry->profile.total.cpu_record_ns += g_get_monotonic_time () * 1000;

  self->debug_current = entry->parent;

  GSK_GPU_FRAME_CLASS (gsk_vulkan_debug_frame_parent_class)->end_node (frame);
}

static GskDebugProfile *
gsk_vulkan_debug_frame_get_profile (GskGpuFrame *frame)
{
  GskVulkanDebugFrame *self = GSK_VULKAN_DEBUG_FRAME (frame);

  if (self->debug_current == NO_ITEM)
    return NULL;

  return &gsk_vulkan_debug_get (&self->debug, self->debug_current)->profile;
}

static void
gsk_vulkan_debug_frame_finalize (GObject *object)
{
  GskVulkanDebugFrame *self = GSK_VULKAN_DEBUG_FRAME (object);
  GskVulkanDevice *device;
  VkDevice vk_device;

  device = GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (GSK_GPU_FRAME (self)));
  vk_device = gsk_vulkan_device_get_vk_device (device);

  g_free (self->timestamp_pool_values);
  g_free (self->timestamp_pool_nodes);
  vkDestroyQueryPool (vk_device,
                      self->vk_timestamp_pool,
                      NULL);

  gsk_vulkan_debug_clear (&self->debug);

  G_OBJECT_CLASS (gsk_vulkan_debug_frame_parent_class)->finalize (object);
}

static void
gsk_vulkan_debug_frame_class_init (GskVulkanDebugFrameClass *klass)
{
  GskVulkanFrameClass *vulkan_frame_class = GSK_VULKAN_FRAME_CLASS (klass);
  GskGpuFrameClass *gpu_frame_class = GSK_GPU_FRAME_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  vulkan_frame_class->submit_ops = gsk_vulkan_debug_frame_submit_ops;

  gpu_frame_class->setup = gsk_vulkan_debug_frame_setup;
  gpu_frame_class->cleanup = gsk_vulkan_debug_frame_cleanup;
  gpu_frame_class->alloc_op = gsk_vulkan_debug_frame_alloc_op;
  gpu_frame_class->start_node = gsk_vulkan_debug_frame_start_node;
  gpu_frame_class->end_node = gsk_vulkan_debug_frame_end_node;
  gpu_frame_class->get_profile = gsk_vulkan_debug_frame_get_profile;

  object_class->finalize = gsk_vulkan_debug_frame_finalize;
}

static void
gsk_vulkan_debug_frame_init (GskVulkanDebugFrame *self)
{
  self->debug_current = NO_ITEM;
  gsk_vulkan_debug_init (&self->debug);
}

