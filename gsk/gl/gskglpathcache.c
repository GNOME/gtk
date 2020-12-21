#include "config.h"

#include "gskglpathcacheprivate.h"
#include "gskstrokeprivate.h"

#define MAX_UNUSED_FRAMES (16 * 5)

typedef struct
{
  GskPath *path;
  GskFillRule fill_rule;
  const GskStroke *stroke;
  guint hash;

  GskStroke stroke_copy;
  graphene_rect_t bounds;
  int texture_id;
  int unused_frames;
} CacheItem;

static guint
compute_hash (CacheItem *item)
{
  guint hash;

  hash = GPOINTER_TO_UINT (item->path);
  hash += item->fill_rule;
  if (item->stroke)
    hash += gsk_stroke_hash (item->stroke);

  return hash;
}

static guint
cache_key_hash (gconstpointer v)
{
  const CacheItem *item = v;

  return item->hash;
}

static gboolean
cache_key_equal (gconstpointer v1,
                 gconstpointer v2)
{
  const CacheItem *item1 = v1;
  const CacheItem *item2 = v2;

  return item1->path == item2->path &&
         item1->fill_rule == item2->fill_rule &&
         (item1->stroke == item2->stroke ||
          (item1->stroke && item2->stroke &&
           gsk_stroke_equal (item1->stroke, item2->stroke)));
}

void
gsk_gl_path_cache_init (GskGLPathCache *self)
{
  self->textures = g_hash_table_new_full (cache_key_hash,
                                          cache_key_equal,
                                          NULL,
                                          g_free);
}

void
gsk_gl_path_cache_free (GskGLPathCache *self,
                        GskGLDriver    *gl_driver)
{
  GHashTableIter iter;
  CacheItem *item;

  g_hash_table_iter_init (&iter, self->textures);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&item))
    {
      gsk_gl_driver_destroy_texture (gl_driver, item->texture_id);
    }

  g_hash_table_unref (self->textures);
  self->textures = NULL;
}

void
gsk_gl_path_cache_begin_frame (GskGLPathCache *self,
                               GskGLDriver    *gl_driver)
{
  GHashTableIter iter;
  CacheItem *item;

  g_hash_table_iter_init (&iter, self->textures);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&item))
    {
      if (item->unused_frames > MAX_UNUSED_FRAMES)
        {
          gsk_gl_driver_destroy_texture (gl_driver, item->texture_id);
          g_hash_table_iter_remove (&iter);
        }
    }
}

int
gsk_gl_path_cache_get_texture_id (GskGLPathCache  *self,
                                  GskPath         *path,
                                  GskFillRule      fill_rule,
                                  const GskStroke *stroke,
                                  graphene_rect_t *out_bounds)
{
  CacheItem key;
  CacheItem *item;

  key.path = path;
  key.fill_rule = fill_rule;
  key.stroke = stroke;
  key.hash = compute_hash (&key);

  item = g_hash_table_lookup (self->textures, &key);
  if (item == NULL)
    return 0;

  item->unused_frames = 0;

  g_assert (item->texture_id != 0);

  *out_bounds = item->bounds;

  return item->texture_id;
}

void
gsk_gl_path_cache_commit (GskGLPathCache        *self,
                          GskPath               *path,
                          GskFillRule            fill_rule,
                          const GskStroke       *stroke,
                          int                    texture_id,
                          const graphene_rect_t *bounds)
{
  CacheItem *item;

  g_assert (self != NULL);
  g_assert (texture_id > 0);

  item = g_new0 (CacheItem, 1);

  item->path = path;
  item->fill_rule = fill_rule;
  if (stroke)
    {
      gsk_stroke_init_copy (&item->stroke_copy, stroke);
      item->stroke = &item->stroke_copy;
    }
  item->hash = compute_hash (item);

  item->unused_frames = 0;
  item->texture_id = texture_id;
  item->bounds = *bounds;

  g_hash_table_add (self->textures, item);
}
