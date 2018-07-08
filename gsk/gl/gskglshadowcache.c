
#include "gskglshadowcacheprivate.h"

typedef struct
{
  GskRoundedRect outline;
  float blur_radius;
} CacheKey;

typedef struct
{
  int texture_id;
  guint used : 1;
} CacheValue;

static gboolean
key_equal (const void *x,
           const void *y)
{
  const CacheKey *a = x;
  const CacheKey *b = y;

  return memcmp (&a->outline, &b->outline, sizeof (GskRoundedRect)) == 0 &&
         a->blur_radius == b->blur_radius;
}

void
gsk_gl_shadow_cache_init (GskGLShadowCache *self)
{
  self->textures = g_hash_table_new_full (g_direct_hash, key_equal, g_free, g_free);
}

void
gsk_gl_shadow_cache_free (GskGLShadowCache *self,
                          GskGLDriver      *gl_driver)
{
  g_hash_table_unref (self->textures);
  self->textures = NULL;
}

void
gsk_gl_shadow_cache_begin_frame (GskGLShadowCache *self,
                                 GskGLDriver      *gl_driver)
{
  GHashTableIter iter;
  CacheKey *key;
  CacheValue *value;

  /* We remove all textures with used = FALSE since those have not been used in the
   * last frame. For all others, we reset the `used` value to FALSE instead and see
   * if they end up with TRUE in the next call to begin_frame. */

  g_hash_table_iter_init (&iter, self->textures);
  while (g_hash_table_iter_next (&iter, (gpointer *)&key, (gpointer *)&value))
    {
      if (!value->used)
        {
          /* Remove */
          gsk_gl_driver_destroy_texture (gl_driver, value->texture_id);
          g_hash_table_iter_remove (&iter);
        }
      else
        {
          value->used = FALSE;
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
  CacheValue *value;

  g_assert (self != NULL);
  g_assert (gl_driver != NULL);
  g_assert (shadow_rect != NULL);

  value = g_hash_table_lookup (self->textures,
                               &(CacheKey){
                                 *shadow_rect,
                                 blur_radius
                               });

  if (value == NULL)
    return 0;

  value->used = TRUE;

  return value->texture_id;
}

void
gsk_gl_shadow_cache_commit (GskGLShadowCache     *self,
                            const GskRoundedRect *shadow_rect,
                            float                 blur_radius,
                            int                   texture_id)
{
  CacheKey *key;
  CacheValue *value;

  g_assert (self != NULL);
  g_assert (shadow_rect != NULL);
  g_assert (texture_id > 0);

  key = g_new0 (CacheKey, 1);
  key->outline = *shadow_rect;
  key->blur_radius = blur_radius;

  value = g_new0 (CacheValue, 1);
  value->used = TRUE;
  value->texture_id = texture_id;

  g_hash_table_insert (self->textures, key, value);
}
