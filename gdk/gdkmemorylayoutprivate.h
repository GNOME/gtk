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
    (format), (width), (height), \
    (stride) * ((height) - 1) + (width) * gdk_memory_format_get_plane_block_bytes (format, 0), \
    { { 0, (stride) }, } \
  }

void                    gdk_memory_layout_init                  (GdkMemoryLayout                *self,
                                                                 GdkMemoryFormat                 format,
                                                                 gsize                           width,
                                                                 gsize                           height,
                                                                 gsize                           align);
gboolean                gdk_memory_layout_try_init              (GdkMemoryLayout                *self,
                                                                 GdkMemoryFormat                 format,
                                                                 gsize                           width,
                                                                 gsize                           height,
                                                                 gsize                           align);
void                    gdk_memory_layout_init_sublayout        (GdkMemoryLayout                *self,
                                                                 const GdkMemoryLayout          *other,
                                                                 const cairo_rectangle_int_t    *area);

gboolean                gdk_memory_layout_is_valid              (const GdkMemoryLayout          *self,
                                                                 GError                        **error);
gboolean                gdk_memory_layout_is_aligned            (const GdkMemoryLayout          *self,
                                                                 gsize                           align);
gboolean                gdk_memory_layout_has_overlap           (const guchar                   *data1,
                                                                 const GdkMemoryLayout          *layout1,
                                                                 const guchar                   *data2,
                                                                 const GdkMemoryLayout          *layout2);
#define gdk_memory_layout_return_val_if_invalid(layout, val) G_STMT_START{\
  GError *_e = NULL; \
  if (!gdk_memory_layout_is_valid (layout, &_e)) \
    { \
      g_return_if_fail_warning (G_LOG_DOMAIN, \
                                G_STRFUNC, \
                                _e->message); \
      g_clear_error (&_e); \
      return (val); \
    } \
}G_STMT_END
#define gdk_memory_layout_return_if_invalid(layout) G_STMT_START{\
  GError *_e = NULL; \
  if (!gdk_memory_layout_is_valid (layout, &_e)) \
    { \
      g_return_if_fail_warning (G_LOG_DOMAIN, \
                                G_STRFUNC, \
                                _e->message); \
      g_clear_error (&_e); \
      return; \
    } \
}G_STMT_END

gsize                   gdk_memory_layout_offset                (const GdkMemoryLayout          *layout,
                                                                 gsize                           plane,
                                                                 gsize                           x,
                                                                 gsize                           y) G_GNUC_PURE;

void                    gdk_memory_copy                         (guchar                         *dest_data,
                                                                 const GdkMemoryLayout          *dest_layout,
                                                                 const guchar                   *src_data,
                                                                 const GdkMemoryLayout          *src_layout);

G_END_DECLS

