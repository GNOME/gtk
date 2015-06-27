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
gtk_render_ops_class_init (GtkRenderOpsClass *klass)
{
  klass->begin_draw_widget = gtk_render_ops_real_begin_draw_widget;
  klass->end_draw_widget = gtk_render_ops_real_end_draw_widget;
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
