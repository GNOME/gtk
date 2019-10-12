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
  GdkTexture *source_texture;
} IconData;

static void
icon_data_free (gpointer p)
{
  g_object_unref (((IconData *)p)->source_texture);
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
gsk_gl_icon_cache_begin_frame (GskGLIconCache *self,
                               GPtrArray      *removed_atlases)
{
  GHashTableIter iter;
  GdkTexture *texture;
  IconData *icon_data;

  /* Drop icons on removed atlases */
  if (removed_atlases->len > 0)
    {
      g_hash_table_iter_init (&iter, self->icons);
      while (g_hash_table_iter_next (&iter, (gpointer *)&texture, (gpointer *)&icon_data))
        {
          if (g_ptr_array_find (removed_atlases, icon_data->atlas, NULL))
            g_hash_table_iter_remove (&iter);
        }
    }
  
  /* Increase frame age of all remaining icons */
  g_hash_table_iter_init (&iter, self->icons);
  while (g_hash_table_iter_next (&iter, (gpointer *)&texture, (gpointer *)&icon_data))
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
           * This way we can revive it when we use it again.
           */
        }
    }
}

static void
upload_regions (GskGLIconCache *self,
                guint           texture_id,
                guint           n_regions,
                GskImageRegion *regions)
{
  int i;

  glBindTexture (GL_TEXTURE_2D, texture_id);
  for (i = 0; i < n_regions; i++)
    {
      glPixelStorei (GL_UNPACK_ROW_LENGTH, regions[i].stride / 4);
      glTexSubImage2D (GL_TEXTURE_2D, 0,
                       regions[i].x, regions[i].y,
                       regions[i].width, regions[i].height,
                       GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                       regions[i].data);
    }
  glPixelStorei (GL_UNPACK_ROW_LENGTH, 0);
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
    GskImageRegion region[5];
    cairo_surface_t *surface;
    guchar *padding;

    gsk_gl_texture_atlases_pack (self->atlases, width + 2, height + 2, &atlas, &packed_x, &packed_y);

    icon_data = g_new0 (IconData, 1);
    icon_data->atlas = atlas;
    icon_data->frame_age = 0;
    icon_data->used = TRUE;
    icon_data->source_texture = g_object_ref (texture);
    graphene_rect_init (&icon_data->texture_rect,
                        (float)(packed_x + 1) / atlas->width,
                        (float)(packed_y + 1) / atlas->height,
                        (float)width / atlas->width,
                        (float)height / atlas->height);

    g_hash_table_insert (self->icons, texture, icon_data);

    /* actually upload the texture */
    surface = gdk_texture_download_surface (texture);
    padding = g_new0 (guchar, 4 * (MAX (width, height) + 2));

    region[0].x = packed_x + 1;
    region[0].y = packed_y + 1;
    region[0].width = width;
    region[0].height = height;
    region[0].stride = cairo_image_surface_get_stride (surface);
    region[0].data = cairo_image_surface_get_data (surface);

    region[1].x = packed_x;
    region[1].y = packed_y;
    region[1].width = width + 2;
    region[1].height = 1;
    region[1].stride = 0;
    region[1].data = padding;

    region[2].x = packed_x;
    region[2].y = packed_y + 1 + height;
    region[2].width = width + 2;
    region[2].height = 1;
    region[2].stride = 0;
    region[2].data = padding;

    region[3].x = packed_x;
    region[3].y = packed_y;
    region[3].width = 1;
    region[3].height = height + 2;
    region[3].stride = 0;
    region[3].data = padding;

    region[4].x = packed_x + 1 + width;
    region[4].y = packed_y;
    region[4].width = 1;
    region[4].height = height + 2;
    region[4].stride = 0;
    region[4].data = padding;

    gdk_gl_context_push_debug_group_printf (gdk_gl_context_get_current (),
                                            "Uploading texture");

    upload_regions (self, atlas->texture_id, 5, region);

    gdk_gl_context_pop_debug_group (gdk_gl_context_get_current ());

    *out_texture_id = atlas->texture_id;
    *out_texture_rect = icon_data->texture_rect;

    cairo_surface_destroy (surface);
    g_free (padding);

#if 0
    {
      static int k;
      const int stride = cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, atlas->width);
      guchar *data = g_malloc (atlas->height * stride);
      cairo_surface_t *s;
      char *filename = g_strdup_printf ("atlas_%u_%d.png", atlas->texture_id, k++);

      glBindTexture (GL_TEXTURE_2D, atlas->texture_id);
      glGetTexImage (GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, data);
      s = cairo_image_surface_create_for_data (data, CAIRO_FORMAT_ARGB32, atlas->width, atlas->height, stride);
      cairo_surface_write_to_png (s, filename);

      cairo_surface_destroy (s);
      g_free (data);
      g_free (filename);
    }
#endif
  }
}
