/*
 * Copyright © 2015 Benjamin Otte <otte@gnome.org>
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

#include "gtkrenderoperationborder.h"

#include <math.h>

#include "gtkcssstyleprivate.h"
#include "gtkrenderborderprivate.h"

G_DEFINE_TYPE (GtkRenderOperationBorder, gtk_render_operation_border, GTK_TYPE_RENDER_OPERATION)

static void
gtk_render_operation_border_get_clip (GtkRenderOperation    *operation,
                                      cairo_rectangle_int_t *clip)
{
  GtkRenderOperationBorder *oper = GTK_RENDER_OPERATION_BORDER (operation);

  clip->x = 0;
  clip->y = 0;
  clip->width = ceil (oper->width);
  clip->height = ceil (oper->height);
}

static void
gtk_render_operation_border_get_matrix (GtkRenderOperation *operation,
                                        cairo_matrix_t     *matrix)
{
  GtkRenderOperationBorder *oper = GTK_RENDER_OPERATION_BORDER (operation);

  cairo_matrix_init_translate (matrix, oper->x, oper->y);
}

static char *
gtk_render_operation_border_describe (GtkRenderOperation *operation)
{
  return g_strdup ("CSS border");
}

static void
gtk_render_operation_border_draw (GtkRenderOperation *operation,
                                  cairo_t            *cr)
{
  GtkRenderOperationBorder *oper = GTK_RENDER_OPERATION_BORDER (operation);

  gtk_css_style_render_border (oper->style,
                               cr,
                               0, 0, 
                               oper->width,
                               oper->height,
                               oper->hidden_side,
                               oper->junction);
}

static void
gtk_render_operation_border_finalize (GObject *object)
{
  GtkRenderOperationBorder *oper = GTK_RENDER_OPERATION_BORDER (object);

  g_object_unref (oper->style);

  G_OBJECT_CLASS (gtk_render_operation_border_parent_class)->finalize (object);
}

static void
gtk_render_operation_border_class_init (GtkRenderOperationBorderClass *klass)
{
  GtkRenderOperationClass *operation_class = GTK_RENDER_OPERATION_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_render_operation_border_finalize;

  operation_class->get_clip = gtk_render_operation_border_get_clip;
  operation_class->get_matrix = gtk_render_operation_border_get_matrix;
  operation_class->describe = gtk_render_operation_border_describe;
  operation_class->draw = gtk_render_operation_border_draw;
}

static void
gtk_render_operation_border_init (GtkRenderOperationBorder *image)
{
}

GtkRenderOperation *
gtk_render_operation_border_new (GtkCssStyle      *style,
                                 double            x,
                                 double            y,
                                 double            width,
                                 double            height,
                                 guint             hidden_side,
                                 GtkJunctionSides  junction)
{
  GtkRenderOperationBorder *result;

  g_return_val_if_fail (GTK_IS_CSS_STYLE (style), NULL);

  result = g_object_new (GTK_TYPE_RENDER_OPERATION_BORDER, NULL);
  
  result->style = g_object_ref (style);
  result->x = x;
  result->y = y;
  result->width = width;
  result->height = height;
  result->hidden_side = hidden_side;
  result->junction = junction;
 
  return GTK_RENDER_OPERATION (result);
}

