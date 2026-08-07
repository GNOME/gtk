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

#pragma once

#include "gdkdrawcontext.h"

#include "gdkcolorstateprivate.h"
#include "gdkmemoryformatprivate.h"

#include "gsk/gsktypes.h"

#include <graphene.h>

G_BEGIN_DECLS

typedef struct _GdkDrawContextFrame GdkDrawContextFrame;

struct _GdkDrawContextFrame
{
  GdkDrawContext *context;

  guint buffer_width;
  guint buffer_height;
  cairo_region_t *damage;
};

struct _GdkDrawContext
{
  GObject parent_instance;
};

struct _GdkDrawContextClass
{
  GObjectClass parent_class;

  gsize                 frame_size;

  void                  (* initialize_frame)                    (GdkDrawContext         *context,
                                                                 GdkDrawContextFrame    *frame);
  void                  (* finalize_frame)                      (GdkDrawContext         *context,
                                                                 GdkDrawContextFrame    *frame);
  gboolean              (* release_frame)                       (GdkDrawContext         *context,
                                                                 GdkDrawContextFrame    *frame);
  void                  (* begin_frame)                         (GdkDrawContext         *context,
                                                                 GdkDrawContextFrame    *frame,
                                                                 gpointer                context_data,
                                                                 GdkColorState         **out_color_state,
                                                                 GdkMemoryDepth         *out_depth);
  void                  (* end_frame)                           (GdkDrawContext         *context,
                                                                 GdkDrawContextFrame    *frame,
                                                                 gpointer                context_data);
  void                  (* empty_frame)                         (GdkDrawContext         *context);
  void                  (* surface_resized)                     (GdkDrawContext         *context);
  gboolean              (* surface_attach)                      (GdkDrawContext         *context,
                                                                 GError                **error);
  void                  (* surface_detach)                      (GdkDrawContext         *context);
};

void                    gdk_draw_context_surface_resized        (GdkDrawContext         *context);

GdkDrawContextFrame *   gdk_draw_context_begin_frame_full       (GdkDrawContext         *context,
                                                                 gpointer                context_data,
                                                                 GskRenderNode          *node,
                                                                 const cairo_region_t   *region);
void                    gdk_draw_context_end_frame_full         (GdkDrawContext         *context,
                                                                 gpointer                context_data);

void                    gdk_draw_context_empty_frame            (GdkDrawContext         *context);

gboolean                gdk_draw_context_attach                 (GdkDrawContext         *self,
                                                                 GError                **error);
void                    gdk_draw_context_detach                 (GdkDrawContext         *self);

GdkDrawContextFrame *   gdk_draw_context_get_current_frame      (GdkDrawContext         *self);
GdkColorState *         gdk_draw_context_get_color_state        (GdkDrawContext         *self);
GdkMemoryDepth          gdk_draw_context_get_depth              (GdkDrawContext         *self);
void                    gdk_draw_context_get_buffer_size        (GdkDrawContext         *self,
                                                                 guint                  *out_width,
                                                                 guint                  *out_height);

const cairo_region_t *  gdk_draw_context_frame_get_damage       (GdkDrawContextFrame    *frame);
void                    gdk_draw_context_frame_add_damage       (GdkDrawContextFrame    *frame,
                                                                 const cairo_region_t   *damage);


G_END_DECLS

