#include "config.h"

#include "gskvulkanmemoryprivate.h"

#include "gskvulkanprivate.h"


struct _GskVulkanMemory
{
  GdkVulkanContext *vulkan;

  gsize size;

  GskVulkanAllocator *allocator;
  GskVulkanAllocation allocation;
};

static GskVulkanAllocator *
gsk_vulkan_allocator_get (GdkVulkanContext   *context,
                          gsize               index,
                          const VkMemoryType *type)
{
  GskVulkanAllocator **allocators = g_object_get_data (G_OBJECT (context), "-gsk-allocators");

  g_assert (index <= 10);

  if (allocators == NULL)
    {
      allocators = g_new0 (GskVulkanAllocator *, 10);
      g_object_set_data (G_OBJECT (context), "-gsk-allocators", allocators);
    }

  if (allocators[index] == NULL)
    {
      allocators[index] = gsk_vulkan_direct_allocator_new (gdk_vulkan_context_get_device (context),
                                                           index,
                                                           type);
      //allocators[index] = gsk_vulkan_stats_allocator_new (allocators[index]);
    }

  return allocators[index];
}

GskVulkanMemory *
gsk_vulkan_memory_new (GdkVulkanContext      *context,
                       uint32_t               allowed_types,
                       VkMemoryPropertyFlags  flags,
                       gsize                  size)
{
  VkPhysicalDeviceMemoryProperties properties;
  GskVulkanMemory *self;
  uint32_t i;

  self = g_new0 (GskVulkanMemory, 1);

  self->size = size;

  vkGetPhysicalDeviceMemoryProperties (gdk_vulkan_context_get_physical_device (context),
                                       &properties);

  for (i = 0; i < properties.memoryTypeCount; i++)
    {
      if (!(allowed_types & (1 << i)))
        continue;

      if ((properties.memoryTypes[i].propertyFlags & flags) == flags)
        break;
  }

  g_assert (i < properties.memoryTypeCount);

  self->allocator = gsk_vulkan_allocator_get (context, i, &properties.memoryTypes[i]);
  gsk_vulkan_alloc (self->allocator, size, &self->allocation);

  return self;
}

void
gsk_vulkan_memory_free (GskVulkanMemory *self)
{
  gsk_vulkan_free (self->allocator, &self->allocation);

  g_free (self);
}

VkDeviceMemory
gsk_vulkan_memory_get_device_memory (GskVulkanMemory *self)
{
  return self->allocation.vk_memory;
}

gboolean
gsk_vulkan_memory_can_map (GskVulkanMemory *self,
                           gboolean         fast)
{
  return self->allocation.map != NULL;
}

guchar *
gsk_vulkan_memory_map (GskVulkanMemory *self)
{
  return self->allocation.map;
}

void
gsk_vulkan_memory_unmap (GskVulkanMemory *self)
{
}

/* {{{ direct allocator ***/

typedef struct _GskVulkanDirectAllocator GskVulkanDirectAllocator;

struct _GskVulkanDirectAllocator
{
  GskVulkanAllocator allocator_class;

  VkDevice device; /* no reference held */
  uint32_t vk_memory_type_index;
  VkMemoryType vk_memory_type;
};

static void
gsk_vulkan_direct_allocator_free_allocator (GskVulkanAllocator *allocator)
{
  g_free (allocator);
}

static void
gsk_vulkan_direct_allocator_alloc (GskVulkanAllocator  *allocator,
                                   VkDeviceSize         size,
                                   GskVulkanAllocation *alloc)
{
  GskVulkanDirectAllocator *self = (GskVulkanDirectAllocator *) allocator;

  GSK_VK_CHECK (vkAllocateMemory, self->device,
                                  &(VkMemoryAllocateInfo) {
                                      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                      .allocationSize = size,
                                      .memoryTypeIndex = self->vk_memory_type_index
                                  },
                                  NULL,
                                  &alloc->vk_memory);

  if ((self->vk_memory_type.propertyFlags & GSK_VULKAN_MEMORY_MAPPABLE) == GSK_VULKAN_MEMORY_MAPPABLE)
    {
        GSK_VK_CHECK (vkMapMemory, self->device,
                                   alloc->vk_memory,
                                   0,
                                   size,
                                   0,
                                   (void **) &alloc->map);
    }

  alloc->offset = 0;
  alloc->size = size;
}

static void
gsk_vulkan_direct_allocator_free (GskVulkanAllocator        *allocator,
                                  const GskVulkanAllocation *alloc)
{
  GskVulkanDirectAllocator *self = (GskVulkanDirectAllocator *) allocator;

  if (alloc->map)
    vkUnmapMemory (self->device, alloc->vk_memory);

  vkFreeMemory (self->device,
                alloc->vk_memory,
                NULL);
}

GskVulkanAllocator *
gsk_vulkan_direct_allocator_new (VkDevice            device,
                                 uint32_t            vk_type_index,
                                 const VkMemoryType *vk_type)
{
  GskVulkanDirectAllocator *self;

  self = g_new0 (GskVulkanDirectAllocator, 1);
  self->allocator_class.free_allocator = gsk_vulkan_direct_allocator_free_allocator;
  self->allocator_class.alloc = gsk_vulkan_direct_allocator_alloc;
  self->allocator_class.free = gsk_vulkan_direct_allocator_free;
  self->device = device;
  self->vk_memory_type_index = vk_type_index;
  self->vk_memory_type = *vk_type;

  return (GskVulkanAllocator *) self;
}

/* }}} */
/* {{{ stats allocator ***/

typedef struct _GskVulkanStatsAllocator GskVulkanStatsAllocator;

struct _GskVulkanStatsAllocator
{
  GskVulkanAllocator allocator_class;

  GskVulkanAllocator *allocator;

  gsize n_alloc;
  gsize n_free;

  gsize n_bytes_requested;
  gsize n_bytes_allocated;
  gsize n_bytes_freed;
};

static void
dump_stats (GskVulkanStatsAllocator *self,
            const char              *reason)
{
  g_printerr ("%s\n", reason);
  g_printerr ("  %zu bytes requested in %zu allocations\n", self->n_bytes_requested, self->n_alloc);
  g_printerr ("  %zu bytes allocated (%.2f%% overhead)\n", self->n_bytes_allocated,
              (self->n_bytes_allocated - self->n_bytes_requested) * 100. / self->n_bytes_requested);
  g_printerr ("  %zu bytes freed in %zu frees\n", self->n_bytes_freed , self->n_free);
  g_printerr ("  %zu bytes remaining in %zu allocations\n",
              self->n_bytes_allocated - self->n_bytes_freed, self->n_alloc - self->n_free);
}

static void
gsk_vulkan_stats_allocator_free_allocator (GskVulkanAllocator *allocator)
{
  GskVulkanStatsAllocator *self = (GskVulkanStatsAllocator *) allocator;

  g_assert (self->n_alloc == self->n_free);
  g_assert (self->n_bytes_allocated == self->n_bytes_freed);

  gsk_vulkan_allocator_free (self->allocator);

  g_free (self);
}

static void
gsk_vulkan_stats_allocator_alloc (GskVulkanAllocator  *allocator,
                                  VkDeviceSize         size,
                                  GskVulkanAllocation *alloc)
{
  GskVulkanStatsAllocator *self = (GskVulkanStatsAllocator *) allocator;

  gsk_vulkan_alloc (self->allocator, size, alloc);

  self->n_alloc++;
  self->n_bytes_requested += size;
  self->n_bytes_allocated += alloc->size;

  dump_stats (self, "alloc()");
}

static void
gsk_vulkan_stats_allocator_free (GskVulkanAllocator        *allocator,
                                 const GskVulkanAllocation *alloc)
{
  GskVulkanStatsAllocator *self = (GskVulkanStatsAllocator *) allocator;

  self->n_free++;
  self->n_bytes_freed += alloc->size;

  gsk_vulkan_free (self->allocator, alloc);

  dump_stats (self, "free()");
}

GskVulkanAllocator *
gsk_vulkan_stats_allocator_new (GskVulkanAllocator *allocator)
{
  GskVulkanStatsAllocator *self;

  self = g_new0 (GskVulkanStatsAllocator, 1);
  self->allocator_class.free_allocator = gsk_vulkan_stats_allocator_free_allocator;
  self->allocator_class.alloc = gsk_vulkan_stats_allocator_alloc;
  self->allocator_class.free = gsk_vulkan_stats_allocator_free;
  self->allocator = allocator;

  return (GskVulkanAllocator *) self;
}

/* }}} */

