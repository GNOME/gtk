/*
 * Copyright © 2014 Red Hat Inc.
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

#ifndef __GTK_RENDER_OPS_PRIVATE_H__
#define __GTK_RENDER_OPS_PRIVATE_H__

#include <cairo.h>

#include <gtk/gtktypes.h>
#include "gtkcssimagebuiltinprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_RENDER_OPS           (gtk_render_ops_get_type ())
#define GTK_RENDER_OPS(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_RENDER_OPS, GtkRenderOps))
#define GTK_RENDER_OPS_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_RENDER_OPS, GtkRenderOpsClass))
#define GTK_IS_RENDER_OPS(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_RENDER_OPS))
#define GTK_IS_RENDER_OPS_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_RENDER_OPS))
#define GTK_RENDER_OPS_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_RENDER_OPS, GtkRenderOpsClass))

typedef struct _GtkRenderOps           GtkRenderOps;
typedef struct _GtkRenderOpsClass      GtkRenderOpsClass;

struct _GtkRenderOps
{
  GObject parent;
};

struct _GtkRenderOpsClass
{
  GObjectClass parent_class;

  cairo_t *     (* begin_draw_widget)                  (GtkRenderOps            *ops,
                                                        GtkWidget               *widget,
                                                        cairo_t                 *cr);
  void          (* end_draw_widget)                    (GtkRenderOps            *ops,
                                                        GtkWidget               *widget,
                                                        cairo_t                 *draw_cr,
                                                        cairo_t                 *original_cr);
void            (* draw_background)                     (GtkRenderOps           *ops,
                                                         GtkCssStyle            *style,
                                                         cairo_t                *cr,
                                                         gdouble                 x,
                                                         gdouble                 y,
                                                         gdouble                 width,
                                                         gdouble                 height,
                                                         GtkJunctionSides        junction);
void            (* draw_border)                         (GtkRenderOps           *ops,
                                                         GtkCssStyle            *style,
                                                         cairo_t                *cr,
                                                         gdouble                 x,
                                                         gdouble                 y,
                                                         gdouble                 width,
                                                         gdouble                 height,
                                                         guint                   hidden_side,
                                                         GtkJunctionSides        junction);
void            (* draw_outline)                        (GtkRenderOps           *ops,
                                                         GtkCssStyle            *style,
                                                         cairo_t                *cr,
                                                         gdouble                 x,
                                                         gdouble                 y,
                                                         gdouble                 width,
                                                         gdouble                 height);
void            (* draw_icon)                           (GtkRenderOps           *ops,
                                                         GtkCssStyle            *style,
                                                         cairo_t                *cr,
                                                         double                  x,
                                                         double                  y,
                                                         double                  width,
                                                         double                  height,
                                                         GtkCssImageBuiltinType  builtin_type);
void            (* draw_icon_surface)                   (GtkRenderOps           *ops,
                                                         GtkCssStyle            *style,
                                                         cairo_t                *cr,
                                                         cairo_surface_t        *surface,
                                                         double                  x,
                                                         double                  y);
};

GType           gtk_render_ops_get_type                (void) G_GNUC_CONST;

/* public API */
void            gtk_cairo_set_render_ops               (cairo_t                 *cr,
                                                        GtkRenderOps            *ops);

/* calls from inside GTK */
cairo_t *       gtk_render_ops_begin_draw_widget       (GtkWidget               *widget,
                                                        cairo_t                 *cr);
void            gtk_render_ops_end_draw_widget         (GtkWidget               *widget,
                                                        cairo_t                 *draw_cr,
                                                        cairo_t                 *original_cr);

void            gtk_render_ops_draw_background          (GtkCssStyle            *style,
                                                         cairo_t                *cr,
                                                         gdouble                 x,
                                                         gdouble                 y,
                                                         gdouble                 width,
                                                         gdouble                 height,
                                                         GtkJunctionSides        junction);
void            gtk_render_ops_draw_border              (GtkCssStyle            *style,
                                                         cairo_t                *cr,
                                                         gdouble                 x,
                                                         gdouble                 y,
                                                         gdouble                 width,
                                                         gdouble                 height,
                                                         guint                   hidden_side,
                                                         GtkJunctionSides        junction);
void            gtk_render_ops_draw_outline             (GtkCssStyle            *style,
                                                         cairo_t                *cr,
                                                         gdouble                 x,
                                                         gdouble                 y,
                                                         gdouble                 width,
                                                         gdouble                 height);
void            gtk_render_ops_draw_icon                (GtkCssStyle            *style,
                                                         cairo_t                *cr,
                                                         double                  x,
                                                         double                  y,
                                                         double                  width,
                                                         double                  height,
                                                         GtkCssImageBuiltinType  builtin_type);
void            gtk_render_ops_draw_icon_surface        (GtkCssStyle            *style,
                                                         cairo_t                *cr,
                                                         cairo_surface_t        *surface,
                                                         double                  x,
                                                         double                  y);

G_END_DECLS

#endif /* __GTK_RENDER_OPS_PRIVATE_H__ */
