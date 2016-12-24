#ifndef __GSK_VULKAN_CLIP_PRIVATE_H__
#define __GSK_VULKAN_CLIP_PRIVATE_H__

#include <gdk/gdk.h>
#include <graphene.h>
#include <gsk/gskroundedrect.h>

G_BEGIN_DECLS

typedef enum {
  /* The whole area is clipped, no drawing is necessary.
   * This can't be handled by return values because for return
   * values we return if clips could even be computed.
   */
  GSK_VULKAN_CLIP_ALL_CLIPPED,
  /* No clipping is necesary, but the clip rect is set
   * to the actual bounds of the underlying framebuffer
   */
  GSK_VULKAN_CLIP_NONE,
  /* The clip is a rectangular area */
  GSK_VULKAN_CLIP_RECT,
  /* The clip is a rounded rectangle, and for every corner
   * corner.width == corner.height is true
   */
  GSK_VULKAN_CLIP_ROUNDED_CIRCULAR,
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

gboolean                gsk_vulkan_clip_intersect_rect                  (GskVulkanClip          *dest,
                                                                         const GskVulkanClip    *src,
                                                                         const graphene_rect_t  *rect) G_GNUC_WARN_UNUSED_RESULT;
gboolean                gsk_vulkan_clip_intersect_rounded_rect          (GskVulkanClip          *dest,
                                                                         const GskVulkanClip    *src,
                                                                         const GskRoundedRect   *rounded) G_GNUC_WARN_UNUSED_RESULT;
gboolean                gsk_vulkan_clip_transform                       (GskVulkanClip          *dest,
                                                                         const GskVulkanClip    *src,
                                                                         const graphene_matrix_t*transform,
                                                                         const graphene_rect_t  *viewport) G_GNUC_WARN_UNUSED_RESULT;

gboolean                gsk_vulkan_clip_contains_rect                   (const GskVulkanClip    *self,
                                                                         const graphene_rect_t  *rect) G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS

#endif /* __GSK_VULKAN_CLIP_PRIVATE_H__ */
