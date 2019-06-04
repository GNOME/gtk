#include "gskgliconcacheprivate.h"
#include "gskgltextureatlasprivate.h"
#include "gdk/gdktextureprivate.h"
#include "gdk/gdkglcontextprivate.h"

#include <epoxy/gl.h>

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

static void
free_atlas (gpointer v)
{
  GskGLTextureAtlas *atlas = v;

  gsk_gl_texture_atlas_free (atlas);

  g_free (atlas);
}

GskGLIconCache *
gsk_gl_icon_cache_new (GdkDisplay *display)
{
  GskGLIconCache *self;

  self = g_new0 (GskGLIconCache, 1);

  self->display = display;

  self->atlases = g_ptr_array_new_with_free_func ((GDestroyNotify)free_atlas);
  self->icons = g_hash_table_new_full (NULL, NULL, NULL, icon_data_free);

  return self;
}

void
gsk_gl_icon_cache_free (GskGLIconCache *self)
{
  guint i, p;

  for (i = 0, p = self->atlases->len; i < p; i ++)
    {
      GskGLTextureAtlas *atlas = g_ptr_array_index (self->atlases, i);

      if (atlas->texture_id != 0)
        {
          glDeleteTextures (1, &atlas->texture_id);
          atlas->texture_id = 0;
        }
    }
  g_ptr_array_free (self->atlases, TRUE);
  g_hash_table_unref (self->icons);

  g_free (self);
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

              gsk_gl_texture_atlas_mark_unused (icon_data->atlas, w + 2, h + 2);
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

          if (atlas->texture_id != 0)
            {
              glDeleteTextures (1, &atlas->texture_id);
              atlas->texture_id = 0;
            }

          g_ptr_array_remove_index_fast (self->atlases, i);
          i --; /* Check the current index again */
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

static guint
create_shared_texture (GskGLIconCache *self,
                       int             width,
                       int             height)
{
  guint texture_id;

  glGenTextures (1, &texture_id);
  glBindTexture (GL_TEXTURE_2D, texture_id);

  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  if (gdk_gl_context_get_use_es (gdk_gl_context_get_current ()))
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  else
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);

  glBindTexture (GL_TEXTURE_2D, 0);

  return texture_id;
}

static void
upload_region_or_else (GskGLIconCache *self,
                       guint           texture_id,
                       GskImageRegion *region)
{
  glBindTexture (GL_TEXTURE_2D, texture_id);
  glTextureSubImage2D (texture_id, 0, region->x, region->y, region->width, region->height,
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
          const int w = icon_data->texture_rect.size.width  * ATLAS_SIZE;
          const int h = icon_data->texture_rect.size.height * ATLAS_SIZE;

          gsk_gl_texture_atlas_mark_used (icon_data->atlas, w + 2, h + 2);
          icon_data->used = TRUE;
        }

      *out_texture_id = icon_data->atlas->texture_id;
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
    cairo_surface_t *padded_surface;

    g_assert (twidth  < ATLAS_SIZE);
    g_assert (theight < ATLAS_SIZE);

    for (i = 0, p = self->atlases->len; i < p; i ++)
      {
        atlas = g_ptr_array_index (self->atlases, i);

        if (gsk_gl_texture_atlas_pack (atlas, twidth + 2, theight + 2, &packed_x, &packed_y))
          {
            packed_x += 1;
            packed_y += 1;
            break;
          }

        atlas = NULL;
      }

    if (!atlas)
      {
        /* No atlas has enough space, so create a new one... */
        atlas = g_malloc (sizeof (GskGLTextureAtlas));
        gsk_gl_texture_atlas_init (atlas, ATLAS_SIZE, ATLAS_SIZE);
        atlas->texture_id = create_shared_texture (self, atlas->width, atlas->height);
        gdk_gl_context_label_object_printf (gdk_gl_context_get_current (),
                                            GL_TEXTURE, atlas->texture_id,
                                            "Icon atlas %d", atlas->texture_id);

        /* Pack it onto that one, which surely has enought space... */
        gsk_gl_texture_atlas_pack (atlas, twidth + 2, theight + 2, &packed_x, &packed_y);
        packed_x += 1;
        packed_y += 1;

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
    padded_surface = pad_surface (surface);
    region.x = packed_x - 1;
    region.y = packed_y - 1;
    region.width = twidth + 2;
    region.height = theight + 2;
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
