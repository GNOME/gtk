#pragma once

#include <gdk/gdk.h>
#include <graphene.h>
#include <gsk/gskroundedrect.h>

G_BEGIN_DECLS

typedef enum {
  GSK_VULKAN_SHADER_CLIP_NONE,
  GSK_VULKAN_SHADER_CLIP_RECT,
  GSK_VULKAN_SHADER_CLIP_ROUNDED
} GskVulkanShaderClip;

typedef enum {
  /* The whole area is clipped, no drawing is necessary.
   * This can't be handled by return values because for return
   * values we return if clips could even be computed.
   */
  GSK_VULKAN_CLIP_ALL_CLIPPED,
  /* No clipping is necessary, but the clip rect is set
   * to the actual bounds of the underlying framebuffer
   */
  GSK_VULKAN_CLIP_NONE,
  /* The clip is a rectangular area */
  GSK_VULKAN_CLIP_RECT,
  /* The clip is a rounded rectangle */
  GSK_VULKAN_CLIP_ROUNDED
} GskVulkanClipComplexity;

typedef struct _GskVulkanClip GskVulkanClip;

struct _GskVulkanClip
{
  GskVulkanClipComplexity type;
  GskRoundedRect          rect;
};

void                    gsk_vulkan_clip_init_empty                      (GskVulkanClip          *clip,
                                                                         const graphene_rect_t  *rect);
void                    gsk_vulkan_clip_init_copy                       (GskVulkanClip          *self,
                                                                         const GskVulkanClip    *src);
void                    gsk_vulkan_clip_init_rect                       (GskVulkanClip          *clip,
                                                                         const graphene_rect_t  *rect);

gboolean                gsk_vulkan_clip_intersect_rect                  (GskVulkanClip          *dest,
                                                                         const GskVulkanClip    *src,
                                                                         const graphene_rect_t  *rect) G_GNUC_WARN_UNUSED_RESULT;
gboolean                gsk_vulkan_clip_intersect_rounded_rect          (GskVulkanClip          *dest,
                                                                         const GskVulkanClip    *src,
                                                                         const GskRoundedRect   *rounded) G_GNUC_WARN_UNUSED_RESULT;
void                    gsk_vulkan_clip_scale                           (GskVulkanClip          *dest,
                                                                         const GskVulkanClip    *src,
                                                                         float                   scale_x,
                                                                         float                   scale_y);
gboolean                gsk_vulkan_clip_transform                       (GskVulkanClip          *dest,
                                                                         const GskVulkanClip    *src,
                                                                         GskTransform           *transform,
                                                                         const graphene_rect_t  *viewport) G_GNUC_WARN_UNUSED_RESULT;

gboolean                gsk_vulkan_clip_contains_rect                   (const GskVulkanClip    *self,
                                                                         const graphene_point_t *offset,
                                                                         const graphene_rect_t  *rect) G_GNUC_WARN_UNUSED_RESULT;
gboolean                gsk_vulkan_clip_may_intersect_rect              (const GskVulkanClip    *self,
                                                                         const graphene_point_t *offset,
                                                                         const graphene_rect_t  *rect) G_GNUC_WARN_UNUSED_RESULT;
GskVulkanShaderClip     gsk_vulkan_clip_get_shader_clip                 (const GskVulkanClip    *self,
                                                                         const graphene_point_t *offset,
                                                                         const graphene_rect_t  *rect);

G_END_DECLS

