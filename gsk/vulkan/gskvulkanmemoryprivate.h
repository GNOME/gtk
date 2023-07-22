#pragma once

#include <gdk/gdk.h>

G_BEGIN_DECLS

#define GSK_VULKAN_MEMORY_MAPPABLE (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | \
                                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | \
                                    VK_MEMORY_PROPERTY_HOST_CACHED_BIT)

typedef struct _GskVulkanAllocator GskVulkanAllocator;
typedef struct _GskVulkanAllocation GskVulkanAllocation;
typedef struct _GskVulkanMemory GskVulkanMemory;

struct _GskVulkanAllocation
{
  VkDeviceMemory vk_memory;
  guchar *map;
  VkDeviceSize offset;
  VkDeviceSize size;
};

struct _GskVulkanAllocator
{
  void                  (* free_allocator)                              (GskVulkanAllocator             *allocator);

  void                  (* alloc)                                       (GskVulkanAllocator             *allocator,
                                                                         VkDeviceSize                    size,
                                                                         GskVulkanAllocation            *out_alloc);
  void                  (* free)                                        (GskVulkanAllocator             *allocator,
                                                                         const GskVulkanAllocation      *alloc);
};

static inline void      gsk_vulkan_alloc                                (GskVulkanAllocator             *allocator,
                                                                         gsize                           size,
                                                                         GskVulkanAllocation            *out_alloc);
static inline void      gsk_vulkan_free                                 (GskVulkanAllocator             *allocator,
                                                                         const GskVulkanAllocation      *alloc);

static inline void      gsk_vulkan_allocator_free                       (GskVulkanAllocator             *allocator);

GskVulkanAllocator *    gsk_vulkan_direct_allocator_new                 (VkDevice                        device,
                                                                         uint32_t                        vk_type_index,
                                                                         const VkMemoryType             *vk_type);
GskVulkanAllocator *    gsk_vulkan_stats_allocator_new                  (GskVulkanAllocator             *allocator);

GskVulkanMemory *       gsk_vulkan_memory_new                           (GdkVulkanContext       *context,
                                                                         uint32_t                allowed_types,
                                                                         VkMemoryPropertyFlags   properties,
                                                                         gsize                   size);
void                    gsk_vulkan_memory_free                          (GskVulkanMemory        *memory);

VkDeviceMemory          gsk_vulkan_memory_get_device_memory             (GskVulkanMemory        *self);

gboolean                gsk_vulkan_memory_can_map                       (GskVulkanMemory        *self,
                                                                         gboolean                fast);
guchar *                gsk_vulkan_memory_map                           (GskVulkanMemory        *self);
void                    gsk_vulkan_memory_unmap                         (GskVulkanMemory        *self);

static inline void
gsk_vulkan_alloc (GskVulkanAllocator  *allocator,
                  VkDeviceSize         size,
                  GskVulkanAllocation *out_alloc)
{
  allocator->alloc (allocator, size, out_alloc);
}

static inline void
gsk_vulkan_free (GskVulkanAllocator        *allocator,
                 const GskVulkanAllocation *alloc)
{
  allocator->free (allocator, alloc);
}

static inline void
gsk_vulkan_allocator_free (GskVulkanAllocator *allocator)
{
  allocator->free_allocator (allocator);
}

G_END_DECLS

