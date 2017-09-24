#include "config.h"

#include "gskvulkanshaderprivate.h"
#include "gskvulkanpipelineprivate.h"

struct _GskVulkanShader
{
  GdkVulkanContext *vulkan;

  GskVulkanShaderType type;
  VkShaderModule vk_shader;
};

GskVulkanShader *
gsk_vulkan_shader_new_from_bytes (GdkVulkanContext     *context,
                                  GskVulkanShaderType   type,
                                  GBytes               *bytes,
                                  GError              **error)
{
  GskVulkanShader *self;
  VkShaderModule shader;
  VkResult res;

  res = GSK_VK_CHECK (vkCreateShaderModule, gdk_vulkan_context_get_device (context),
                                            &(VkShaderModuleCreateInfo) {
                                                .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                                .codeSize = g_bytes_get_size (bytes),
                                                .pCode = (uint32_t *) g_bytes_get_data (bytes, NULL),
                                            },
                                            NULL,
                                            &shader);
  if (res != VK_SUCCESS)
    {
      /* Someone invent better error categories plz */
      g_set_error (error, GDK_VULKAN_ERROR, GDK_VULKAN_ERROR_UNSUPPORTED,
                   "Could not create shader: %s", gdk_vulkan_strerror (res));
      return NULL;
    }

  self = g_slice_new0 (GskVulkanShader);

  self->vulkan = g_object_ref (context);
  self->type = type;
  self->vk_shader = shader;

  return self;
}

GskVulkanShader *
gsk_vulkan_shader_new_from_resource (GdkVulkanContext     *context,
                                     GskVulkanShaderType   type,
                                     const char           *resource_name,
                                     GError              **error)
{
  GskVulkanShader *self;
  GBytes *bytes;
  GError *local_error = NULL;
  char *path;

  path = g_strconcat ("/org/gtk/libgsk/vulkan/",
                      resource_name, 
                      type == GSK_VULKAN_SHADER_VERTEX ? ".vert.spv" : ".frag.spv",
                      NULL);
  bytes = g_resources_lookup_data (path, 0, &local_error);
  g_free (path);
  if (bytes == NULL)
    {
      GSK_NOTE (VULKAN, g_printerr ("Error loading shader data: %s\n", local_error->message));
      g_propagate_error (error, local_error);
      return NULL;
    }

  self = gsk_vulkan_shader_new_from_bytes (context, type, bytes, error);
  g_bytes_unref (bytes);

  return self;
}

void
gsk_vulkan_shader_free (GskVulkanShader *self)
{
  vkDestroyShaderModule (gdk_vulkan_context_get_device (self->vulkan),
                         self->vk_shader,
                         NULL);

  g_object_unref (self->vulkan);

  g_slice_free (GskVulkanShader, self);
}

GskVulkanShaderType
gsk_vulkan_shader_get_type (GskVulkanShader *shader)
{
  return shader->type;
}

VkShaderModule
gsk_vulkan_shader_get_module (GskVulkanShader *shader)
{
  return shader->vk_shader;
}

