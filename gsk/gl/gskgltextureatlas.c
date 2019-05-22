
#include "gskgltextureatlasprivate.h"


void
gsk_gl_texture_atlas_init (GskGLTextureAtlas *self,
                           int                width,
                           int                height)
{
  memset (self, 0, sizeof (*self));

  self->image.texture_id = 0;
  self->width = width;
  self->height = height;

  /* TODO: We might want to change the strategy about the amount of
   *       nodes here? stb_rect_pack.h says with is optimal. */
  self->nodes = g_malloc0 (sizeof (struct stbrp_node) * width);
  stbrp_init_target (&self->context,
                     width, height,
                     self->nodes,
                     width);
}

void
gsk_gl_texture_atlas_free (GskGLTextureAtlas *self)
{
  g_clear_pointer (&self->nodes, g_free);
}

void
gsk_gl_texture_atlas_mark_unused (GskGLTextureAtlas *self,
                                  int                width,
                                  int                height)
{
  self->unused_pixels += (width * height);
}


void
gsk_gl_texture_atlas_mark_used (GskGLTextureAtlas *self,
                                int                width,
                                int                height)
{
  self->unused_pixels -= (width * height);

  g_assert (self->unused_pixels >= 0);
}

gboolean
gsk_gl_texture_atlas_pack (GskGLTextureAtlas *self,
                           int                width,
                           int                height,
                           int               *out_x,
                           int               *out_y)
{
  stbrp_rect rect;

  g_assert (out_x);
  g_assert (out_y);

  rect.w = width;
  rect.h = height;

  stbrp_pack_rects (&self->context, &rect, 1);

  if (rect.was_packed)
    {
      *out_x = rect.x;
      *out_y = rect.y;
    }

  return rect.was_packed;
}

double
gsk_gl_texture_atlas_get_unused_ratio (const GskGLTextureAtlas *self)
{
  if (self->unused_pixels > 0)
    return (double)(self->unused_pixels) / (double)(self->width * self->height);

  return 0.0;
}

