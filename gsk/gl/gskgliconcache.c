#include "gskgliconcacheprivate.h"
#include "gskgltextureatlasprivate.h"
#include "gdk/gdktextureprivate.h"

#define ATLAS_SIZE    (1024)
#define MAX_FRAME_AGE (5 * 60)
#define MAX_UNUSED_RATIO 0.8

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

void
gsk_gl_icon_cache_init (GskGLIconCache *self,
                        GskRenderer    *renderer,
                        GskGLDriver    *gl_driver)
{
  self->renderer = renderer;
  self->gl_driver = gl_driver;

  self->atlases = g_ptr_array_new ();
  self->icons = g_hash_table_new_full (NULL, NULL, NULL, icon_data_free);
}

void
gsk_gl_icon_cache_free (GskGLIconCache *self)
{
  guint i, p;

  for (i = 0, p = self->atlases->len; i < p; i ++)
    {
      GskGLTextureAtlas *atlas = g_ptr_array_index (self->atlases, i);

      gsk_gl_texture_atlas_free (atlas);
      g_free (atlas);
    }
  g_ptr_array_free (self->atlases, TRUE);

  g_hash_table_unref (self->icons);
}

void
gsk_gl_icon_cache_begin_frame (GskGLIconCache *self)
{
  gint i, p;
  GHashTableIter iter;
  GdkTexture *texture;
  IconData *icon_data;

  /* Increase frame age of all icons */
  g_hash_table_iter_init (&iter, self->icons);
  while (g_hash_table_iter_next (&iter, (gpointer *)&texture, (gpointer *)&icon_data))
    {
      icon_data->frame_age ++;

      if (icon_data->frame_age > MAX_FRAME_AGE)
        {

          if (icon_data->used)
            {
              const int w = icon_data->texture_rect.size.width  * ATLAS_SIZE;
              const int h = icon_data->texture_rect.size.height * ATLAS_SIZE;

              gsk_gl_texture_atlas_mark_unused (icon_data->atlas, w, h);
              icon_data->used = FALSE;
            }
          /* We do NOT remove the icon here. Instead, We wait until we drop the entire atlas.
           * This way we can revive it when we use it again. */
        }
    }

  for (i = 0, p = self->atlases->len; i < p; i ++)
    {
      GskGLTextureAtlas *atlas = g_ptr_array_index (self->atlases, i);

      if (gsk_gl_texture_atlas_get_unused_ratio (atlas) > MAX_UNUSED_RATIO)
        {
          g_hash_table_iter_init (&iter, self->icons);
          while (g_hash_table_iter_next (&iter, (gpointer *)&texture, (gpointer *)&icon_data))
            {
              if (icon_data->atlas == atlas)
                g_hash_table_iter_remove (&iter);
            }

          g_ptr_array_remove_index_fast (self->atlases, i);
          i --; /* Check the current index again */
        }
    }
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
          const int w = icon_data->texture_rect.size.width  * ATLAS_SIZE;
          const int h = icon_data->texture_rect.size.height * ATLAS_SIZE;

          gsk_gl_texture_atlas_mark_used (icon_data->atlas, w, h);
          icon_data->used = TRUE;
        }

      *out_texture_id = icon_data->atlas->image.texture_id;
      *out_texture_rect = icon_data->texture_rect;
      return;
    }

  /* texture not on any atlas yet. Find a suitable one. */
  {
    const int twidth = gdk_texture_get_width (texture);
    const int theight = gdk_texture_get_height (texture);
    int packed_x, packed_y;
    GskGLTextureAtlas *atlas = NULL;
    guint i, p;
    GskImageRegion region;
    cairo_surface_t *surface;

    g_assert (twidth  < ATLAS_SIZE);
    g_assert (theight < ATLAS_SIZE);

    for (i = 0, p = self->atlases->len; i < p; i ++)
      {
        atlas = g_ptr_array_index (self->atlases, i);

        if (gsk_gl_texture_atlas_pack (atlas, twidth, theight, &packed_x, &packed_y))
          break;

        atlas = NULL;
      }

    if (!atlas)
      {
        /* No atlas has enough space, so create a new one... */
        atlas = g_malloc (sizeof (GskGLTextureAtlas));
        gsk_gl_texture_atlas_init (atlas, ATLAS_SIZE, ATLAS_SIZE);
        gsk_gl_image_create (&atlas->image, self->gl_driver, atlas->width, atlas->height);
        /* Pack it onto that one, which surely has enought space... */
        gsk_gl_texture_atlas_pack (atlas, twidth, theight, &packed_x, &packed_y);

        g_ptr_array_add (self->atlases, atlas);
      }

    icon_data = g_new0 (IconData, 1);
    icon_data->atlas = atlas;
    icon_data->frame_age = 0;
    icon_data->used = TRUE;
    graphene_rect_init (&icon_data->texture_rect,
                        (float)packed_x / ATLAS_SIZE,
                        (float)packed_y / ATLAS_SIZE,
                        (float)twidth   / ATLAS_SIZE,
                        (float)theight  / ATLAS_SIZE);

    g_hash_table_insert (self->icons, texture, icon_data);

    /* actually upload the texture */
    surface = gdk_texture_download_surface (texture);
    region.x = packed_x;
    region.y = packed_y;
    region.width = twidth;
    region.height = theight;
    region.data = cairo_image_surface_get_data (surface);

    gsk_gl_image_upload_region (&atlas->image, self->gl_driver, &region);

    *out_texture_id = atlas->image.texture_id;
    *out_texture_rect = icon_data->texture_rect;

    cairo_surface_destroy (surface);

#if 0
    /* Some obvious debugging */
    static int k;
    gsk_gl_image_write_to_png (&atlas->image, self->gl_driver,
                               g_strdup_printf ("icon%d.png", k ++));
#endif
  }
}
