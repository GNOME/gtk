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

#include "gtkrenderoperation.h"

G_DEFINE_TYPE (GtkRenderOperation, gtk_render_operation, G_TYPE_OBJECT)

static void
gtk_render_operation_real_get_clip (GtkRenderOperation    *operation,
                                    cairo_rectangle_int_t *clip)
{
  clip->x = clip->y = clip->width = clip->height = 0;
}

static void
gtk_render_operation_real_get_matrix (GtkRenderOperation *operation,
                                      cairo_matrix_t     *matrix)
{
  cairo_matrix_init_identity (matrix);
}

static char *
gtk_render_operation_real_describe (GtkRenderOperation *operation)
{
  return g_strdup (G_OBJECT_TYPE_NAME (operation));
}

static void
gtk_render_operation_real_draw (GtkRenderOperation *operation,
                                cairo_t            *cr)
{
}

static void
gtk_render_operation_class_init (GtkRenderOperationClass *klass)
{
  klass->get_clip = gtk_render_operation_real_get_clip;
  klass->get_matrix = gtk_render_operation_real_get_matrix;
  klass->describe = gtk_render_operation_real_describe;
  klass->draw = gtk_render_operation_real_draw;
}

static void
gtk_render_operation_init (GtkRenderOperation *image)
{
}

void
gtk_render_operation_get_clip (GtkRenderOperation    *operation,
                               cairo_rectangle_int_t *clip)
{
  g_return_if_fail (GTK_IS_RENDER_OPERATION (operation));
  g_return_if_fail (clip != NULL);

  GTK_RENDER_OPERATION_GET_CLASS (operation)->get_clip (operation, clip);
}

void
gtk_render_operation_get_matrix (GtkRenderOperation *operation,
                                 cairo_matrix_t     *matrix)
{
  g_return_if_fail (GTK_IS_RENDER_OPERATION (operation));
  g_return_if_fail (matrix != NULL);

  GTK_RENDER_OPERATION_GET_CLASS (operation)->get_matrix (operation, matrix);
}

char *
gtk_render_operation_describe (GtkRenderOperation *operation)
{
  g_return_val_if_fail (GTK_IS_RENDER_OPERATION (operation), NULL);

  return GTK_RENDER_OPERATION_GET_CLASS (operation)->describe (operation);
}

void
gtk_render_operation_draw (GtkRenderOperation *operation,
                           cairo_t            *cr)
{
  g_return_if_fail (GTK_IS_RENDER_OPERATION (operation));
  g_return_if_fail (cr != NULL);

  GTK_RENDER_OPERATION_GET_CLASS (operation)->draw (operation, cr);
}

