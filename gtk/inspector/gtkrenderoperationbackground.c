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

#include "gtkrenderoperationbackground.h"

#include <math.h>

#include "gtkcssstyleprivate.h"
#include "gtkcssshadowsvalueprivate.h"
#include "gtkrenderbackgroundprivate.h"

G_DEFINE_TYPE (GtkRenderOperationBackground, gtk_render_operation_background, GTK_TYPE_RENDER_OPERATION)

static void
gtk_render_operation_background_get_clip (GtkRenderOperation    *operation,
                                          cairo_rectangle_int_t *clip)
{
  GtkRenderOperationBackground *oper = GTK_RENDER_OPERATION_BACKGROUND (operation);
  GtkBorder extents;

  _gtk_css_shadows_value_get_extents (gtk_css_style_get_value (oper->style,
                                                               GTK_CSS_PROPERTY_BOX_SHADOW),
                                      &extents);

  clip->x = -extents.left;
  clip->y = -extents.right;
  clip->width = ceil (oper->width) + extents.left + extents.right;
  clip->height = ceil (oper->height) + extents.top + extents.bottom;
}

static void
gtk_render_operation_background_get_matrix (GtkRenderOperation *operation,
                                            cairo_matrix_t     *matrix)
{
  GtkRenderOperationBackground *oper = GTK_RENDER_OPERATION_BACKGROUND (operation);

  cairo_matrix_init_translate (matrix, oper->x, oper->y);
}

static char *
gtk_render_operation_background_describe (GtkRenderOperation *operation)
{
  return g_strdup ("CSS background");
}

static void
gtk_render_operation_background_draw (GtkRenderOperation *operation,
                                      cairo_t            *cr)
{
  GtkRenderOperationBackground *oper = GTK_RENDER_OPERATION_BACKGROUND (operation);

  gtk_css_style_render_background (oper->style,
                                   cr,
                                   0, 0, 
                                   oper->width,
                                   oper->height,
                                   oper->junction);
}

static void
gtk_render_operation_background_finalize (GObject *object)
{
  GtkRenderOperationBackground *oper = GTK_RENDER_OPERATION_BACKGROUND (object);

  g_object_unref (oper->style);

  G_OBJECT_CLASS (gtk_render_operation_background_parent_class)->finalize (object);
}

static void
gtk_render_operation_background_class_init (GtkRenderOperationBackgroundClass *klass)
{
  GtkRenderOperationClass *operation_class = GTK_RENDER_OPERATION_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_render_operation_background_finalize;

  operation_class->get_clip = gtk_render_operation_background_get_clip;
  operation_class->get_matrix = gtk_render_operation_background_get_matrix;
  operation_class->describe = gtk_render_operation_background_describe;
  operation_class->draw = gtk_render_operation_background_draw;
}

static void
gtk_render_operation_background_init (GtkRenderOperationBackground *image)
{
}

GtkRenderOperation *
gtk_render_operation_background_new (GtkCssStyle      *style,
                                     double            x,
                                     double            y,
                                     double            width,
                                     double            height,
                                     GtkJunctionSides  junction)
{
  GtkRenderOperationBackground *result;

  g_return_val_if_fail (GTK_IS_CSS_STYLE (style), NULL);

  result = g_object_new (GTK_TYPE_RENDER_OPERATION_BACKGROUND, NULL);
  
  result->style = g_object_ref (style);
  result->x = x;
  result->y = y;
  result->width = width;
  result->height = height;
  result->junction = junction;
 
  return GTK_RENDER_OPERATION (result);
}

