/* gsknglglyphlibraryprivate.h
 *
 * Copyright 2020 Christian Hergert <chergert@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef __GSK_NGL_GLYPH_LIBRARY_PRIVATE_H__
#define __GSK_NGL_GLYPH_LIBRARY_PRIVATE_H__

#include <pango/pango.h>

#include "gskngltexturelibraryprivate.h"

G_BEGIN_DECLS

#define GSK_TYPE_GL_GLYPH_LIBRARY (gsk_ngl_glyph_library_get_type())

typedef struct _GskNglGlyphKey
{
  PangoFont *font;
  PangoGlyph glyph;
  guint xshift : 2;
  guint yshift : 2;
  guint scale  : 28; /* times 1024 */
} GskNglGlyphKey;

typedef struct _GskNglGlyphValue
{
  GskNglTextureAtlasEntry entry;
  PangoRectangle ink_rect;
} GskNglGlyphValue;

#if GLIB_SIZEOF_VOID_P == 8
G_STATIC_ASSERT (sizeof (GskNglGlyphKey) == 16);
#elif GLIB_SIZEOF_VOID_P == 4
G_STATIC_ASSERT (sizeof (GskNglGlyphKey) == 12);
#endif

G_DECLARE_FINAL_TYPE (GskNglGlyphLibrary, gsk_ngl_glyph_library, GSK, NGL_GLYPH_LIBRARY, GskNglTextureLibrary)

struct _GskNglGlyphLibrary
{
  GskNglTextureLibrary parent_instance;
  guint8 *surface_data;
  gsize surface_data_len;
  struct {
    GskNglGlyphKey key;
    const GskNglGlyphValue *value;
  } front[256];
};

GskNglGlyphLibrary *gsk_ngl_glyph_library_new (GskNglDriver            *driver);
gboolean            gsk_ngl_glyph_library_add (GskNglGlyphLibrary      *self,
                                               GskNglGlyphKey          *key,
                                               const GskNglGlyphValue **out_value);

static inline guint
gsk_ngl_glyph_library_lookup_or_add (GskNglGlyphLibrary      *self,
                                     const GskNglGlyphKey    *key,
                                     const GskNglGlyphValue **out_value)
{
  GskNglTextureAtlasEntry *entry;
  guint front_index = ((key->glyph << 2) | key->xshift) & 0xFF;

  if (memcmp (key, &self->front[front_index], sizeof *key) == 0)
    {
      *out_value = self->front[front_index].value;
    }
  else if (gsk_ngl_texture_library_lookup ((GskNglTextureLibrary *)self, key, &entry))
    {
      *out_value = (GskNglGlyphValue *)entry;
      self->front[front_index].key = *key;
      self->front[front_index].value = *out_value;
    }
  else
    {
      GskNglGlyphKey *k = g_slice_copy (sizeof *key, key);
      g_object_ref (k->font);
      gsk_ngl_glyph_library_add (self, k, out_value);
      self->front[front_index].key = *key;
      self->front[front_index].value = *out_value;
    }

  return GSK_NGL_TEXTURE_ATLAS_ENTRY_TEXTURE (*out_value);
}

G_END_DECLS

#endif /* __GSK_NGL_GLYPH_LIBRARY_PRIVATE_H__ */
