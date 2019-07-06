#include "gskgliconcacheprivate.h"
#include "gskgltextureatlasprivate.h"
#include "gdk/gdktextureprivate.h"
#include "gdk/gdkglcontextprivate.h"

#include <epoxy/gl.h>

#define MAX_FRAME_AGE (5 * 60)

typedef struct
{
  graphene_rect_t texture_rect;
  GskGLTextureAtlas *atlas;
  int frame_age; /* Number of frames this icon is unused */
  guint used: 1;
} IconData;

static void
icon_data_free (gpointer p)
{
  g_free (p);
}

GskGLIconCache *
gsk_gl_icon_cache_new (GdkDisplay *display,
                       GskGLTextureAtlases *atlases)
{
  GskGLIconCache *self;

  self = g_new0 (GskGLIconCache, 1);

  self->display = display;
  self->icons = g_hash_table_new_full (NULL, NULL, NULL, icon_data_free);
  self->atlases = gsk_gl_texture_atlases_ref (atlases);
  self->ref_count = 1;

  return self;
}

GskGLIconCache *
gsk_gl_icon_cache_ref (GskGLIconCache *self)
{
  self->ref_count++;

  return self;
}

void
gsk_gl_icon_cache_unref (GskGLIconCache *self)
{
  g_assert (self->ref_count > 0);

  if (self->ref_count == 1)
    {
      gsk_gl_texture_atlases_unref (self->atlases);
      g_hash_table_unref (self->icons);
      g_free (self);
      return;
    }

  self->ref_count--;
}

void
gsk_gl_icon_cache_begin_frame (GskGLIconCache *self)
{
  GHashTableIter iter;
  GdkTexture *texture;
  IconData *icon_data;

  /* Increase frame age of all icons */
  g_hash_table_iter_init (&iter, self->icons);
  while (g_hash_table_iter_next (&iter, (gpointer *)&texture, (gpointer *)&icon_data))
    {
      guint pos;

      if (!g_ptr_array_find (self->atlases->atlases, icon_data->atlas, &pos))
        {
          g_hash_table_iter_remove (&iter);
        }
      else
        {
          icon_data->frame_age ++;

          if (icon_data->frame_age > MAX_FRAME_AGE)
            {

              if (icon_data->used)
                {
                  const int w = icon_data->texture_rect.size.width  * icon_data->atlas->width;
                  const int h = icon_data->texture_rect.size.height * icon_data->atlas->height;

                  gsk_gl_texture_atlas_mark_unused (icon_data->atlas, w + 2, h + 2);
                  icon_data->used = FALSE;
                }
              /* We do NOT remove the icon here. Instead, We wait until we drop the entire atlas.
               * This way we can revive it when we use it again. */
            }
        }
    }
}

/* FIXME: this could probably be done more efficiently */
static cairo_surface_t *
pad_surface (cairo_surface_t *surface)
{
  cairo_surface_t *padded;
  cairo_t *cr;
  cairo_pattern_t *pattern;
  cairo_matrix_t matrix;

  padded = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                       cairo_image_surface_get_width (surface) + 2,
                                       cairo_image_surface_get_height (surface) + 2);

  cr = cairo_create (padded);

  pattern = cairo_pattern_create_for_surface (surface);
  cairo_pattern_set_extend (pattern, CAIRO_EXTEND_PAD);

  cairo_matrix_init_translate (&matrix, -1, -1);
  cairo_pattern_set_matrix (pattern, &matrix);

  cairo_set_source (cr, pattern);
  cairo_paint (cr);

  cairo_destroy (cr);
  cairo_pattern_destroy (pattern);

  return padded;
}

static void
upload_region_or_else (GskGLIconCache *self,
                       guint           texture_id,
                       GskImageRegion *region)
{
  glBindTexture (GL_TEXTURE_2D, texture_id);
  glTexSubImage2D (GL_TEXTURE_2D, 0, region->x, region->y, region->width, region->height,
                   GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, region->data);
}

void
gsk_gl_icon_cache_lookup_or_add (GskGLIconCache  *self,
                                 GdkTexture      *texture,
                                 int             *out_texture_id,
                                 graphene_rect_t *out_texture_rect)
{
  IconData *icon_data = g_hash_table_lookup (self->icons, texture);

  if (icon_data)
    {
      icon_data->frame_age = 0;
      if (!icon_data->used)
        {
          const int w = icon_data->texture_rect.size.width  * icon_data->atlas->width;
          const int h = icon_data->texture_rect.size.height * icon_data->atlas->height;

          gsk_gl_texture_atlas_mark_used (icon_data->atlas, w + 2, h + 2);
          icon_data->used = TRUE;
        }

      *out_texture_id = icon_data->atlas->texture_id;
      *out_texture_rect = icon_data->texture_rect;
      return;
    }

  /* texture not on any atlas yet. Find a suitable one. */
  {
    const int width = gdk_texture_get_width (texture);
    const int height = gdk_texture_get_height (texture);
    GskGLTextureAtlas *atlas = NULL;
    int packed_x = 0;
    int packed_y = 0;
    GskImageRegion region;
    cairo_surface_t *surface;
    cairo_surface_t *padded_surface;

    gsk_gl_texture_atlases_pack (self->atlases, width + 2, height + 2, &atlas, &packed_x, &packed_y);

    icon_data = g_new0 (IconData, 1);
    icon_data->atlas = atlas;
    icon_data->frame_age = 0;
    icon_data->used = TRUE;
    graphene_rect_init (&icon_data->texture_rect,
                        (float)(packed_x + 1) / atlas->width,
                        (float)(packed_y + 1) / atlas->height,
                        (float)width / atlas->width,
                        (float)height / atlas->height);

    g_hash_table_insert (self->icons, texture, icon_data);

    /* actually upload the texture */
    surface = gdk_texture_download_surface (texture);
    padded_surface = pad_surface (surface);
    region.x = packed_x;
    region.y = packed_y;
    region.width = width + 2;
    region.height = height + 2;
    region.data = cairo_image_surface_get_data (padded_surface);

    gdk_gl_context_push_debug_group_printf (gdk_gl_context_get_current (),
                                            "Uploading texture");

    upload_region_or_else (self, atlas->texture_id, &region);

    gdk_gl_context_pop_debug_group (gdk_gl_context_get_current ());

    *out_texture_id = atlas->texture_id;
    *out_texture_rect = icon_data->texture_rect;

    cairo_surface_destroy (surface);
    cairo_surface_destroy (padded_surface);

#if 0
    /* Some obvious debugging */
    static int k;
    gsk_gl_image_write_to_png (&atlas->image, self->gl_driver,
                               g_strdup_printf ("icon%d.png", k ++));
#endif
  }
}
