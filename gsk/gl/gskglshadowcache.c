
#include "gskglshadowcacheprivate.h"

#define MAX_UNUSED_FRAMES (16 * 5)

typedef struct
{
  GskRoundedRect outline;
  float blur_radius;
} CacheKey;

typedef struct
{
  GskRoundedRect outline;
  float blur_radius;

  int texture_id;
  int unused_frames;
} CacheItem;

static gboolean
key_equal (const void *x,
           const void *y)
{
  const CacheKey *a = x;
  const CacheKey *b = y;

  return a->blur_radius == b->blur_radius &&
         graphene_size_equal (&a->outline.corner[0], &b->outline.corner[0]) &&
         graphene_size_equal (&a->outline.corner[1], &b->outline.corner[1]) &&
         graphene_size_equal (&a->outline.corner[2], &b->outline.corner[2]) &&
         graphene_size_equal (&a->outline.corner[3], &b->outline.corner[3]) &&
         graphene_rect_equal (&a->outline.bounds, &b->outline.bounds);
}

void
gsk_gl_shadow_cache_init (GskGLShadowCache *self)
{
  self->textures = g_array_new (FALSE, TRUE, sizeof (CacheItem));
}

void
gsk_gl_shadow_cache_free (GskGLShadowCache *self,
                          GskGLDriver      *gl_driver)
{
  guint i, p;

  for (i = 0, p = self->textures->len; i < p; i ++)
    {
      const CacheItem *item = &g_array_index (self->textures, CacheItem, i);

      gsk_gl_driver_destroy_texture (gl_driver, item->texture_id);
    }

  g_array_free (self->textures, TRUE);
  self->textures = NULL;
}

void
gsk_gl_shadow_cache_begin_frame (GskGLShadowCache *self,
                                 GskGLDriver      *gl_driver)
{
  guint i, p;

  for (i = 0, p = self->textures->len; i < p; i ++)
    {
      CacheItem *item = &g_array_index (self->textures, CacheItem, i);

      if (item->unused_frames > MAX_UNUSED_FRAMES)
        {
          gsk_gl_driver_destroy_texture (gl_driver, item->texture_id);
          g_array_remove_index_fast (self->textures, i);
          p --;
          i --;
        }
      else
        {
          item->unused_frames ++;
        }
    }
}

/* XXX
 * The offset origin should always be at 0/0, or the blur radius should just go
 * away since it defines the origin position anyway?
 */
int
gsk_gl_shadow_cache_get_texture_id (GskGLShadowCache     *self,
                                    GskGLDriver          *gl_driver,
                                    const GskRoundedRect *shadow_rect,
                                    float                 blur_radius)
{
  CacheItem *item= NULL;
  guint i;

  g_assert (self != NULL);
  g_assert (gl_driver != NULL);
  g_assert (shadow_rect != NULL);

  for (i = 0; i < self->textures->len; i ++)
    {
      CacheItem *k = &g_array_index (self->textures, CacheItem, i);

      if (key_equal (&(CacheKey){*shadow_rect, blur_radius},
                     &(CacheKey){k->outline, k->blur_radius}))
        {
          item = k;
          break;
        }
    }

  if (item == NULL)
    return 0;

  item->unused_frames = 0;

  g_assert (item->texture_id != 0);

  return item->texture_id;
}

void
gsk_gl_shadow_cache_commit (GskGLShadowCache     *self,
                            const GskRoundedRect *shadow_rect,
                            float                 blur_radius,
                            int                   texture_id)
{
  CacheItem *item;

  g_assert (self != NULL);
  g_assert (shadow_rect != NULL);
  g_assert (texture_id > 0);

  g_array_set_size (self->textures, self->textures->len + 1);
  item = &g_array_index (self->textures, CacheItem, self->textures->len - 1);

  item->outline = *shadow_rect;
  item->blur_radius = blur_radius;
  item->unused_frames = 0;
  item->texture_id = texture_id;
}
