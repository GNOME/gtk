/*
 * Copyright © 2011 Red Hat Inc.
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

#include "config.h"

#include "gtkrenderopsprivate.h"

#include "gtkrenderbackgroundprivate.h"
#include "gtkrenderborderprivate.h"
#include "gtkrendericonprivate.h"

G_DEFINE_TYPE (GtkRenderOps, gtk_render_ops, G_TYPE_OBJECT)

static cairo_t *
gtk_render_ops_real_begin_draw_widget (GtkRenderOps *ops,
                                       GtkWidget    *widget,
                                       cairo_t      *cr)
{
  return cairo_reference (cr);
}

static void
gtk_render_ops_real_end_draw_widget (GtkRenderOps *ops,
                                     GtkWidget    *widget,
                                     cairo_t      *draw_cr,
                                     cairo_t      *original_cr)
{
  cairo_destroy (draw_cr);
}

static void
gtk_render_ops_real_draw_background (GtkRenderOps     *ops,
                                     GtkCssStyle      *style,
                                     cairo_t          *cr,
                                     gdouble           x,
                                     gdouble           y,
                                     gdouble           width,
                                     gdouble           height,
                                     GtkJunctionSides  junction)
{
  gtk_css_style_render_background (style, cr, x, y, width, height, junction);
}

void
gtk_render_ops_real_draw_border (GtkRenderOps     *ops,
                                 GtkCssStyle      *style,
                                 cairo_t          *cr,
                                 gdouble           x,
                                 gdouble           y,
                                 gdouble           width,
                                 gdouble           height,
                                 guint             hidden_side,
                                 GtkJunctionSides  junction)
{
  gtk_css_style_render_border (style, cr, x, y, width, height, hidden_side, junction);
}

void
gtk_render_ops_real_draw_outline (GtkRenderOps *ops,
                                  GtkCssStyle  *style,
                                  cairo_t      *cr,
                                  gdouble       x,
                                  gdouble       y,
                                  gdouble       width,
                                  gdouble       height)
{
  gtk_css_style_render_outline (style, cr, x, y, width, height);
}

void
gtk_render_ops_real_draw_icon (GtkRenderOps           *ops,
                               GtkCssStyle            *style,
                               cairo_t                *cr,
                               double                  x,
                               double                  y,
                               double                  width,
                               double                  height,
                               GtkCssImageBuiltinType  builtin_type)
{
  gtk_css_style_render_icon (style, cr, x, y, width, height, builtin_type);
}

void
gtk_render_ops_real_draw_icon_surface (GtkRenderOps    *ops,
                                       GtkCssStyle     *style,
                                       cairo_t         *cr,
                                       cairo_surface_t *surface,
                                       double           x,
                                       double           y)
{
  gtk_css_style_render_icon_surface (style, cr, surface, x, y);
}

static void
gtk_render_ops_class_init (GtkRenderOpsClass *klass)
{
  klass->begin_draw_widget = gtk_render_ops_real_begin_draw_widget;
  klass->end_draw_widget = gtk_render_ops_real_end_draw_widget;
  klass->draw_background = gtk_render_ops_real_draw_background;
  klass->draw_border = gtk_render_ops_real_draw_border;
  klass->draw_outline = gtk_render_ops_real_draw_outline;
  klass->draw_icon = gtk_render_ops_real_draw_icon;
  klass->draw_icon_surface = gtk_render_ops_real_draw_icon_surface;
}

static void
gtk_render_ops_init (GtkRenderOps *image)
{
}

static const cairo_user_data_key_t render_ops_key;

static GtkRenderOps *
gtk_cairo_get_render_ops (cairo_t *cr)
{
  return cairo_get_user_data (cr, &render_ops_key);
}

/* public API */
void
gtk_cairo_set_render_ops (cairo_t      *cr,
                          GtkRenderOps *ops)
{
  if (ops)
    {
      g_object_ref (ops);
      cairo_set_user_data (cr, &render_ops_key, ops, g_object_unref);
    }
  else
    {
      cairo_set_user_data (cr, &render_ops_key, NULL, NULL);
    }
}

cairo_t *
gtk_render_ops_begin_draw_widget (GtkWidget *widget,
                                  cairo_t   *cr)
{
  GtkRenderOps *ops;

  ops = gtk_cairo_get_render_ops (cr);
  if (ops == NULL)
    return gtk_render_ops_real_begin_draw_widget (NULL, widget, cr);

  return GTK_RENDER_OPS_GET_CLASS (ops)->begin_draw_widget (ops, widget, cr);
}

void
gtk_render_ops_end_draw_widget (GtkWidget *widget,
                                cairo_t   *draw_cr,
                                cairo_t   *original_cr)
{
  GtkRenderOps *ops;

  ops = gtk_cairo_get_render_ops (original_cr);
  if (ops == NULL)
    {
      gtk_render_ops_real_end_draw_widget (NULL, widget, draw_cr, original_cr);
      return;
    }

  GTK_RENDER_OPS_GET_CLASS (ops)->end_draw_widget (ops, widget, draw_cr, original_cr);
}

void
gtk_render_ops_draw_background (GtkCssStyle      *style,
                                cairo_t          *cr,
                                gdouble           x,
                                gdouble           y,
                                gdouble           width,
                                gdouble           height,
                                GtkJunctionSides  junction)
{
  GtkRenderOps *ops;

  ops = gtk_cairo_get_render_ops (cr);
  if (ops == NULL)
    {
      gtk_render_ops_real_draw_background (NULL, style, cr, x, y, width, height, junction);
      return;
    }

  GTK_RENDER_OPS_GET_CLASS (ops)->draw_background (ops, style, cr, x, y, width, height, junction);
}

void
gtk_render_ops_draw_border (GtkCssStyle      *style,
                            cairo_t          *cr,
                            gdouble           x,
                            gdouble           y,
                            gdouble           width,
                            gdouble           height,
                            guint             hidden_side,
                            GtkJunctionSides  junction)
{
  GtkRenderOps *ops;

  ops = gtk_cairo_get_render_ops (cr);
  if (ops == NULL)
    {
      gtk_render_ops_real_draw_border (NULL, style, cr, x, y, width, height, hidden_side, junction);
      return;
    }

  GTK_RENDER_OPS_GET_CLASS (ops)->draw_border (ops, style, cr, x, y, width, height, hidden_side, junction);
}

void
gtk_render_ops_draw_outline (GtkCssStyle *style,
                             cairo_t     *cr,
                             gdouble      x,
                             gdouble      y,
                             gdouble      width,
                             gdouble      height)
{
  GtkRenderOps *ops;

  ops = gtk_cairo_get_render_ops (cr);
  if (ops == NULL)
    {
      gtk_render_ops_real_draw_outline (NULL, style, cr, x, y, width, height);
      return;
    }

  GTK_RENDER_OPS_GET_CLASS (ops)->draw_outline (ops, style, cr, x, y, width, height);
}

void
gtk_render_ops_draw_icon (GtkCssStyle            *style,
                          cairo_t                *cr,
                          double                  x,
                          double                  y,
                          double                  width,
                          double                  height,
                          GtkCssImageBuiltinType  builtin_type)
{
  GtkRenderOps *ops;

  ops = gtk_cairo_get_render_ops (cr);
  if (ops == NULL)
    {
      gtk_render_ops_real_draw_icon (NULL, style, cr, x, y, width, height, builtin_type);
      return;
    }

  GTK_RENDER_OPS_GET_CLASS (ops)->draw_icon (ops, style, cr, x, y, width, height, builtin_type);
}

void
gtk_render_ops_draw_icon_surface (GtkCssStyle     *style,
                                  cairo_t         *cr,
                                  cairo_surface_t *surface,
                                  double           x,
                                  double           y)
{
  GtkRenderOps *ops;

  ops = gtk_cairo_get_render_ops (cr);
  if (ops == NULL)
    {
      gtk_render_ops_real_draw_icon_surface (NULL, style, cr, surface, x, y);
      return;
    }

  GTK_RENDER_OPS_GET_CLASS (ops)->draw_icon_surface (ops, style, cr, surface, x, y);
}

