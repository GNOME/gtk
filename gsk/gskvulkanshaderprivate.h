#ifndef __GSK_VULKAN_SHADER_PRIVATE_H__
#define __GSK_VULKAN_SHADER_PRIVATE_H__

#include <gdk/gdk.h>

G_BEGIN_DECLS

typedef enum {
  GSK_VULKAN_SHADER_VERTEX,
  GSK_VULKAN_SHADER_FRAGMENT
} GskVulkanShaderType;

typedef struct _GskVulkanShader GskVulkanShader;

#define GST_VULKAN_SHADER_STAGE_CREATE_INFO(shader) \
  (VkPipelineShaderStageCreateInfo) { \
  .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, \
  .stage = gsk_vulkan_shader_get_type (shader) == GSK_VULKAN_SHADER_VERTEX ? VK_SHADER_STAGE_VERTEX_BIT : VK_SHADER_STAGE_FRAGMENT_BIT, \
  .module = gsk_vulkan_shader_get_module (shader), \
  .pName = "main", \
}

GskVulkanShader *       gsk_vulkan_shader_new_from_bytes                (GdkVulkanContext       *context,
                                                                         GskVulkanShaderType     type,
                                                                         GBytes                 *bytes,
                                                                         GError                **error);
GskVulkanShader *       gsk_vulkan_shader_new_from_resource             (GdkVulkanContext       *context,
                                                                         GskVulkanShaderType     type,
                                                                         const char             *resource_name,
                                                                         GError                **error);
void                    gsk_vulkan_shader_free                          (GskVulkanShader        *shader);

GskVulkanShaderType     gsk_vulkan_shader_get_type                      (GskVulkanShader        *shader);
VkShaderModule          gsk_vulkan_shader_get_module                    (GskVulkanShader        *shader);

G_END_DECLS

#endif /* __GSK_VULKAN_SHADER_PRIVATE_H__ */
