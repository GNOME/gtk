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

#include "gtkrenderoperationoutline.h"

#include <math.h>

#include "gtkcssstyleprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkrenderborderprivate.h"

G_DEFINE_TYPE (GtkRenderOperationOutline, gtk_render_operation_outline, GTK_TYPE_RENDER_OPERATION)

static void
gtk_render_operation_outline_get_clip (GtkRenderOperation    *operation,
                                       cairo_rectangle_int_t *clip)
{
  GtkRenderOperationOutline *oper = GTK_RENDER_OPERATION_OUTLINE (operation);
  double offset, border_width;

  border_width = _gtk_css_number_value_get (gtk_css_style_get_value (oper->style, GTK_CSS_PROPERTY_OUTLINE_WIDTH), 100);
  offset = _gtk_css_number_value_get (gtk_css_style_get_value (oper->style, GTK_CSS_PROPERTY_OUTLINE_OFFSET), 100);
  
  clip->x = floor (- border_width - offset);
  clip->y = floor (- border_width - offset);
  clip->width = ceil (oper->width + border_width + offset) - clip->x;
  clip->height = ceil (oper->height + border_width + offset) - clip->y;
}

static void
gtk_render_operation_outline_get_matrix (GtkRenderOperation *operation,
                                         cairo_matrix_t     *matrix)
{
  GtkRenderOperationOutline *oper = GTK_RENDER_OPERATION_OUTLINE (operation);

  cairo_matrix_init_translate (matrix, oper->x, oper->y);
}

static char *
gtk_render_operation_outline_describe (GtkRenderOperation *operation)
{
  return g_strdup ("CSS outline");
}

static void
gtk_render_operation_outline_draw (GtkRenderOperation *operation,
                                   cairo_t            *cr)
{
  GtkRenderOperationOutline *oper = GTK_RENDER_OPERATION_OUTLINE (operation);

  gtk_css_style_render_outline (oper->style,
                                cr,
                                0, 0, 
                                oper->width,
                                oper->height);
}

static void
gtk_render_operation_outline_finalize (GObject *object)
{
  GtkRenderOperationOutline *oper = GTK_RENDER_OPERATION_OUTLINE (object);

  g_object_unref (oper->style);

  G_OBJECT_CLASS (gtk_render_operation_outline_parent_class)->finalize (object);
}

static void
gtk_render_operation_outline_class_init (GtkRenderOperationOutlineClass *klass)
{
  GtkRenderOperationClass *operation_class = GTK_RENDER_OPERATION_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_render_operation_outline_finalize;

  operation_class->get_clip = gtk_render_operation_outline_get_clip;
  operation_class->get_matrix = gtk_render_operation_outline_get_matrix;
  operation_class->describe = gtk_render_operation_outline_describe;
  operation_class->draw = gtk_render_operation_outline_draw;
}

static void
gtk_render_operation_outline_init (GtkRenderOperationOutline *image)
{
}

GtkRenderOperation *
gtk_render_operation_outline_new (GtkCssStyle      *style,
                                  double            x,
                                  double            y,
                                  double            width,
                                  double            height)
{
  GtkRenderOperationOutline *result;

  g_return_val_if_fail (GTK_IS_CSS_STYLE (style), NULL);

  result = g_object_new (GTK_TYPE_RENDER_OPERATION_OUTLINE, NULL);
  
  result->style = g_object_ref (style);
  result->x = x;
  result->y = y;
  result->width = width;
  result->height = height;
 
  return GTK_RENDER_OPERATION (result);
}

