#include "config.h"

#include "gskvulkanclipprivate.h"

#include "gskroundedrectprivate.h"
#include "gsktransform.h"

void
gsk_vulkan_clip_init_empty (GskVulkanClip         *clip,
                            const graphene_rect_t *rect)
{
  clip->type = GSK_VULKAN_CLIP_NONE;
  gsk_rounded_rect_init_from_rect (&clip->rect, rect, 0);
}

static void
gsk_vulkan_clip_init_copy (GskVulkanClip *self,
                           const GskVulkanClip *src)
{
  self->type = src->type;
  gsk_rounded_rect_init_copy (&self->rect, &src->rect);
}

gboolean
gsk_vulkan_clip_intersect_rect (GskVulkanClip         *dest,
                                const GskVulkanClip   *src,
                                const graphene_rect_t *rect)
{
  if (graphene_rect_contains_rect (rect, &src->rect.bounds))
    {
      gsk_vulkan_clip_init_copy (dest, src);
      return TRUE;
    }
  if (!graphene_rect_intersection (rect, &src->rect.bounds, NULL))
    {
      dest->type = GSK_VULKAN_CLIP_ALL_CLIPPED;
      return TRUE;
    }

  switch (src->type)
    {
    case GSK_VULKAN_CLIP_ALL_CLIPPED:
      dest->type = GSK_VULKAN_CLIP_ALL_CLIPPED;
      break;

    case GSK_VULKAN_CLIP_NONE:
      gsk_vulkan_clip_init_copy (dest, src);
      if (graphene_rect_intersection (&dest->rect.bounds, rect, &dest->rect.bounds))
        dest->type = GSK_VULKAN_CLIP_RECT;
      else
        dest->type = GSK_VULKAN_CLIP_ALL_CLIPPED;
      break;

    case GSK_VULKAN_CLIP_RECT:
      gsk_vulkan_clip_init_copy (dest, src);
      if (!graphene_rect_intersection (&dest->rect.bounds, rect, &dest->rect.bounds))
        dest->type = GSK_VULKAN_CLIP_ALL_CLIPPED;
      break;

    case GSK_VULKAN_CLIP_ROUNDED_CIRCULAR:
    case GSK_VULKAN_CLIP_ROUNDED:
      if (gsk_rounded_rect_contains_rect (&src->rect, rect))
        {
          dest->type = GSK_VULKAN_CLIP_RECT;
          gsk_rounded_rect_init_from_rect (&dest->rect, rect, 0);
        }
      else
        {
          /* some points of rect are inside src's rounded rect,
           * some are outside. */
          /* XXX: If the 2 rects don't intersect on rounded corners,
           * we could actually compute a new clip here.
           */
          return FALSE;
        }
      break;

    default:
      g_assert_not_reached ();
      return FALSE;
    }

  return TRUE;
}

gboolean
gsk_vulkan_clip_intersect_rounded_rect (GskVulkanClip        *dest,
                                        const GskVulkanClip  *src,
                                        const GskRoundedRect *rounded)
{
  if (gsk_rounded_rect_contains_rect (rounded, &src->rect.bounds))
    {
      gsk_vulkan_clip_init_copy (dest, src);
      return TRUE;
    }
  if (!graphene_rect_intersection (&rounded->bounds, &src->rect.bounds, NULL))
    {
      dest->type = GSK_VULKAN_CLIP_ALL_CLIPPED;
      return TRUE;
    }

  switch (src->type)
    {
    case GSK_VULKAN_CLIP_ALL_CLIPPED:
      dest->type = GSK_VULKAN_CLIP_ALL_CLIPPED;
      break;

    case GSK_VULKAN_CLIP_NONE:
      dest->type = gsk_rounded_rect_is_circular (&dest->rect) ? GSK_VULKAN_CLIP_ROUNDED_CIRCULAR : GSK_VULKAN_CLIP_ROUNDED;
      gsk_rounded_rect_init_copy (&dest->rect, rounded);
      break;

    case GSK_VULKAN_CLIP_RECT:
      if (graphene_rect_contains_rect (&src->rect.bounds, &rounded->bounds))
        {
          dest->type = gsk_rounded_rect_is_circular (rounded) ? GSK_VULKAN_CLIP_ROUNDED_CIRCULAR : GSK_VULKAN_CLIP_ROUNDED;
          gsk_rounded_rect_init_copy (&dest->rect, rounded);
          return TRUE;
        }
      /* some points of rect are inside src's rounded rect,
       * some are outside. */
      /* XXX: If the 2 rects don't intersect on rounded corners,
       * we could actually compute a new clip here.
       */
      return FALSE;

    case GSK_VULKAN_CLIP_ROUNDED_CIRCULAR:
    case GSK_VULKAN_CLIP_ROUNDED:
      if (gsk_rounded_rect_contains_rect (&src->rect, &rounded->bounds))
        {
          dest->type = gsk_rounded_rect_is_circular (rounded) ? GSK_VULKAN_CLIP_ROUNDED_CIRCULAR : GSK_VULKAN_CLIP_ROUNDED;
          gsk_rounded_rect_init_copy (&dest->rect, rounded);
          return TRUE;
        }
      /* XXX: Can be improved for the case where one of the rects is a slighty shrunk version of the other */
      return FALSE;

    default:
      g_assert_not_reached ();
      return FALSE;
    }

  return TRUE;
}

gboolean
gsk_vulkan_clip_transform (GskVulkanClip         *dest,
                           const GskVulkanClip   *src,
                           GskTransform          *transform,
                           const graphene_rect_t *viewport)
{
  switch (src->type)
    {
    default:
      g_assert_not_reached();
      return FALSE;

    case GSK_VULKAN_CLIP_ALL_CLIPPED:
      gsk_vulkan_clip_init_copy (dest, src);
      return TRUE;

    case GSK_VULKAN_CLIP_NONE:
      gsk_vulkan_clip_init_empty (dest, viewport);
      return TRUE;

    case GSK_VULKAN_CLIP_RECT:
    case GSK_VULKAN_CLIP_ROUNDED_CIRCULAR:
    case GSK_VULKAN_CLIP_ROUNDED:
      switch (gsk_transform_get_category (transform))
        {
        case GSK_TRANSFORM_CATEGORY_IDENTITY:
          gsk_vulkan_clip_init_copy (dest, src);
          return TRUE;

        case GSK_TRANSFORM_CATEGORY_2D_TRANSLATE:
          {
            float dx, dy;

            gsk_transform_to_translate (transform, &dx, &dy);
            gsk_vulkan_clip_init_copy (dest, src);
            dest->rect.bounds.origin.x -= dx;
            dest->rect.bounds.origin.y -= dy;
          }
          return TRUE;

        case GSK_TRANSFORM_CATEGORY_2D_AFFINE:
          {
            float dx, dy, scale_x, scale_y;

            gsk_transform_to_affine (transform, &scale_x, &scale_y, &dx, &dy);
            scale_x = 1. / scale_x;
            scale_y = 1. / scale_y;
            gsk_vulkan_clip_init_copy (dest, src);
            dest->rect.bounds.origin.x = (dest->rect.bounds.origin.x - dx) * scale_x;
            dest->rect.bounds.origin.y = (dest->rect.bounds.origin.y - dy) * scale_y;
            dest->rect.bounds.size.width *= scale_x;
            dest->rect.bounds.size.height *= scale_y;
            if (src->type != GSK_VULKAN_CLIP_RECT)
              {
                dest->rect.corner[0].width *= scale_x;
                dest->rect.corner[0].height *= scale_y;
                dest->rect.corner[1].width *= scale_x;
                dest->rect.corner[1].height *= scale_y;
                dest->rect.corner[2].width *= scale_x;
                dest->rect.corner[2].height *= scale_y;
                dest->rect.corner[3].width *= scale_x;
                dest->rect.corner[3].height *= scale_y;
              }
          }
          return TRUE;

        case GSK_TRANSFORM_CATEGORY_UNKNOWN:
        case GSK_TRANSFORM_CATEGORY_ANY:
        case GSK_TRANSFORM_CATEGORY_3D:
        case GSK_TRANSFORM_CATEGORY_2D:
        default:
          return FALSE;
        }
    }
}

gboolean
gsk_vulkan_clip_contains_rect (const GskVulkanClip   *self,
                               const graphene_rect_t *rect)
{
  switch (self->type)
    {
    default:
      g_assert_not_reached();
    case GSK_VULKAN_CLIP_ALL_CLIPPED:
      return FALSE;

    case GSK_VULKAN_CLIP_NONE:
      return TRUE;

    case GSK_VULKAN_CLIP_RECT:
      return graphene_rect_contains_rect (&self->rect.bounds, rect);

    case GSK_VULKAN_CLIP_ROUNDED_CIRCULAR:
    case GSK_VULKAN_CLIP_ROUNDED:
      return gsk_rounded_rect_contains_rect (&self->rect, rect);
    }
}
