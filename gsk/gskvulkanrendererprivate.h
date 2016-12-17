#ifndef __GSK_VULKAN_RENDERER_PRIVATE_H__
#define __GSK_VULKAN_RENDERER_PRIVATE_H__

#include <vulkan/vulkan.h>
#include <gsk/gskrenderer.h>

#include "gsk/gskvulkanimageprivate.h"

G_BEGIN_DECLS

#define GSK_TYPE_VULKAN_RENDERER (gsk_vulkan_renderer_get_type ())

#define GSK_VULKAN_RENDERER(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GSK_TYPE_VULKAN_RENDERER, GskVulkanRenderer))
#define GSK_IS_VULKAN_RENDERER(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GSK_TYPE_VULKAN_RENDERER))
#define GSK_VULKAN_RENDERER_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GSK_TYPE_VULKAN_RENDERER, GskVulkanRendererClass))
#define GSK_IS_VULKAN_RENDERER_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GSK_TYPE_VULKAN_RENDERER))
#define GSK_VULKAN_RENDERER_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GSK_TYPE_VULKAN_RENDERER, GskVulkanRendererClass))

typedef struct _GskVulkanRenderer                GskVulkanRenderer;
typedef struct _GskVulkanRendererClass           GskVulkanRendererClass;

GType gsk_vulkan_renderer_get_type (void) G_GNUC_CONST;

GskVulkanImage *        gsk_vulkan_renderer_ref_texture_image           (GskVulkanRenderer      *self,
                                                                         GskTexture             *texture,
                                                                         GskVulkanUploader      *uploader);

G_END_DECLS

#endif /* __GSK_VULKAN_RENDERER_PRIVATE_H__ */
