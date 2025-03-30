/*
 * Copyright © 2025 Benjamin Otte
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#pragma once

#include "gdkenums.h"
#include "gdktypes.h"

#define GDK_MEMORY_MAX_PLANES 4

typedef struct _GdkMemoryLayout GdkMemoryLayout;

struct _GdkMemoryLayout
{
  GdkMemoryFormat format;
  gsize width;
  gsize height;
  gsize size;
  struct {
    gsize offset;
    gsize stride;
  } planes[GDK_MEMORY_MAX_PLANES];
};

#define GDK_MEMORY_LAYOUT_SIMPLE(format,width,height,stride) \
  (GdkMemoryLayout) { \
    (format), (width), (height), (stride) * (height), \
    { { 0, (stride) }, } \
  }

void                    gdk_memory_layout_init                  (GdkMemoryLayout                *self,
                                                                 GdkMemoryFormat                 format,
                                                                 gsize                           width,
                                                                 gsize                           height,
                                                                 gsize                           align);

gsize                   gdk_memory_layout_offset                (const GdkMemoryLayout          *layout,
                                                                 gsize                           plane,
                                                                 gsize                           x,
                                                                 gsize                           y) G_GNUC_PURE;

G_END_DECLS

