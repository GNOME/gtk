/* gskngliconlibraryprivate.h
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

#ifndef __GSK_NGL_ICON_LIBRARY_PRIVATE_H__
#define __GSK_NGL_ICON_LIBRARY_PRIVATE_H__

#include <pango/pango.h>

#include "gskngltexturelibraryprivate.h"

G_BEGIN_DECLS

#define GSK_TYPE_GL_ICON_LIBRARY (gsk_ngl_icon_library_get_type())

typedef struct _GskNglIconData
{
  GskNglTextureAtlasEntry entry;
  GdkTexture *source_texture;
} GskNglIconData;

G_DECLARE_FINAL_TYPE (GskNglIconLibrary, gsk_ngl_icon_library, GSK, NGL_ICON_LIBRARY, GskNglTextureLibrary)

GskNglIconLibrary *gsk_ngl_icon_library_new (GskNglDriver          *driver);
void               gsk_ngl_icon_library_add (GskNglIconLibrary     *self,
                                             GdkTexture            *key,
                                             const GskNglIconData **out_value);

static inline void
gsk_ngl_icon_library_lookup_or_add (GskNglIconLibrary     *self,
                                    GdkTexture            *key,
                                    const GskNglIconData **out_value)
{
  GskNglTextureAtlasEntry *entry;

  if G_LIKELY (gsk_ngl_texture_library_lookup ((GskNglTextureLibrary *)self, key, &entry))
    *out_value = (GskNglIconData *)entry;
  else
    gsk_ngl_icon_library_add (self, key, out_value);
}

G_END_DECLS

#endif /* __GSK_NGL_ICON_LIBRARY_PRIVATE_H__ */
