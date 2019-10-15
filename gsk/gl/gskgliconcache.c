#include "gskgliconcacheprivate.h"
#include "gskgltextureatlasprivate.h"
#include "gdk/gdktextureprivate.h"
#include "gdk/gdkglcontextprivate.h"

#include <epoxy/gl.h>

#define MAX_FRAME_AGE (5 * 60)

static void
icon_data_free (gpointer p)
{
  IconData *icon_data = p;

  gdk_texture_clear_render_data (icon_data->source_texture);
  g_object_unref (icon_data->source_texture);
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
            {
              g_hash_table_iter_remove (&iter);
            }
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
              const int width = gdk_texture_get_width (icon_data->source_texture);
              const int height = gdk_texture_get_height (icon_data->source_texture);
              gsk_gl_texture_atlas_mark_unused (icon_data->atlas, width + 2, height + 2);
              icon_data->used = FALSE;
            }

          /* We do NOT remove the icon here. Instead, We wait until we drop the entire atlas.
           * This way we can revive it when we use it again.
           */
        }
    }
}

void
gsk_gl_icon_cache_lookup_or_add (GskGLIconCache  *self,
                                 GdkTexture      *texture,
                                 const IconData **out_icon_data)
{
  IconData *icon_data;

  icon_data = gdk_texture_get_render_data (texture, self);

  if (!icon_data)
    icon_data = g_hash_table_lookup (self->icons, texture);

  if (icon_data)
    {
      icon_data->frame_age = 0;
      if (!icon_data->used)
        {
          const int width = gdk_texture_get_width (texture);
          const int height = gdk_texture_get_height (texture);

          gsk_gl_texture_atlas_mark_used (icon_data->atlas, width + 2, height + 2);
          icon_data->used = TRUE;
        }

      *out_icon_data = icon_data;
      return;
    }

  /* texture not on any atlas yet. Find a suitable one. */
  {
    const int width = gdk_texture_get_width (texture);
    const int height = gdk_texture_get_height (texture);
    GskGLTextureAtlas *atlas = NULL;
    int packed_x = 0;
    int packed_y = 0;
    cairo_surface_t *surface;
    unsigned char *surface_data;

    gsk_gl_texture_atlases_pack (self->atlases, width + 2, height + 2, &atlas, &packed_x, &packed_y);

    icon_data = g_new0 (IconData, 1);
    icon_data->atlas = atlas;
    icon_data->frame_age = 0;
    icon_data->used = TRUE;
    icon_data->texture_id = atlas->texture_id;
    icon_data->source_texture = g_object_ref (texture);
    icon_data->x = (float)(packed_x + 1) / atlas->width;
    icon_data->y = (float)(packed_y + 1) / atlas->width;
    icon_data->x2 = icon_data->x + (float)width / atlas->width;
    icon_data->y2 = icon_data->y + (float)height / atlas->height;

    g_hash_table_insert (self->icons, texture, icon_data);

    /* actually upload the texture */
    surface = gdk_texture_download_surface (texture);
    surface_data = cairo_image_surface_get_data (surface);
    gdk_gl_context_push_debug_group_printf (gdk_gl_context_get_current (),
                                            "Uploading texture");
    glBindTexture (GL_TEXTURE_2D, atlas->texture_id);

    glTexSubImage2D (GL_TEXTURE_2D, 0,
                     packed_x + 1, packed_y + 1,
                     width, height,
                     GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                     surface_data);
    /* Padding top */
    glTexSubImage2D (GL_TEXTURE_2D, 0,
                     packed_x + 1, packed_y,
                     width, 1,
                     GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                     surface_data);
    /* Padding left */
    glTexSubImage2D (GL_TEXTURE_2D, 0,
                     packed_x, packed_y + 1,
                     1, height,
                     GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                     surface_data);
    /* Padding top left */
    glTexSubImage2D (GL_TEXTURE_2D, 0,
                     packed_x, packed_y,
                     1, 1,
                     GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                     surface_data);

    /* Padding right */
    glPixelStorei (GL_UNPACK_ROW_LENGTH, width);
    glPixelStorei (GL_UNPACK_SKIP_PIXELS, width - 1);
    glTexSubImage2D (GL_TEXTURE_2D, 0,
                     packed_x + width + 1, packed_y + 1,
                     1, height,
                     GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                     surface_data);
    /* Padding top right */
    glTexSubImage2D (GL_TEXTURE_2D, 0,
                     packed_x + width + 1, packed_y,
                     1, 1,
                     GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                     surface_data);
    /* Padding bottom */
    glPixelStorei (GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei (GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei (GL_UNPACK_SKIP_ROWS, height - 1);
    glTexSubImage2D (GL_TEXTURE_2D, 0,
                     packed_x + 1, packed_y + 1 + height,
                     width, 1,
                     GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                     surface_data);
    /* Padding bottom left */
    glTexSubImage2D (GL_TEXTURE_2D, 0,
                     packed_x, packed_y + 1 + height,
                     1, 1,
                     GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                     surface_data);
    /* Padding bottom right */
    glPixelStorei (GL_UNPACK_ROW_LENGTH, width);
    glPixelStorei (GL_UNPACK_SKIP_PIXELS, width - 1);
    glTexSubImage2D (GL_TEXTURE_2D, 0,
                     packed_x + 1 + width, packed_y + 1 + height,
                     1, 1,
                     GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                     surface_data);

    /* Reset this */
    glPixelStorei (GL_UNPACK_SKIP_PIXELS, 0);
    glPixelStorei (GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei (GL_UNPACK_SKIP_ROWS, 0);

    gdk_gl_context_pop_debug_group (gdk_gl_context_get_current ());

    gdk_texture_set_render_data (texture, self, icon_data, NULL);

    *out_icon_data = icon_data;

    cairo_surface_destroy (surface);

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
