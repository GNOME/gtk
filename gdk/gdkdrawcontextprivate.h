/* GDK - The GIMP Drawing Kit
 *
 * gdkdrawcontext.h: base class for rendering system support
 *
 * Copyright © 2016  Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GDK_DRAW_CONTEXT_PRIVATE__
#define __GDK_DRAW_CONTEXT_PRIVATE__

#include "gdkdrawcontext.h"

G_BEGIN_DECLS

struct _GdkDrawContext
{
  GObject parent_instance;
};

struct _GdkDrawContextClass
{
  GObjectClass parent_class;

  void                  (* begin_frame)                         (GdkDrawContext         *context,
                                                                 cairo_region_t         *update_area);
  void                  (* end_frame)                           (GdkDrawContext         *context,
                                                                 cairo_region_t         *painted);
  void                  (* surface_resized)                     (GdkDrawContext         *context);
};

void                    gdk_draw_context_surface_resized        (GdkDrawContext         *context);

G_END_DECLS

#endif /* __GDK__DRAW_CONTEXT_PRIVATE__ */
