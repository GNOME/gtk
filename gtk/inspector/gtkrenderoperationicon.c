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

#include "gtkrenderoperationicon.h"

#include <math.h>

#include "gtkcssstyleprivate.h"
#include "gtkrendericonprivate.h"

G_DEFINE_TYPE (GtkRenderOperationIcon, gtk_render_operation_icon, GTK_TYPE_RENDER_OPERATION)

static void
gtk_render_operation_icon_get_clip (GtkRenderOperation    *operation,
                                      cairo_rectangle_int_t *clip)
{
  GtkRenderOperationIcon *oper = GTK_RENDER_OPERATION_ICON (operation);

  clip->x = 0;
  clip->y = 0;
  clip->width = ceil (oper->width);
  clip->height = ceil (oper->height);
}

static void
gtk_render_operation_icon_get_matrix (GtkRenderOperation *operation,
                                        cairo_matrix_t     *matrix)
{
  GtkRenderOperationIcon *oper = GTK_RENDER_OPERATION_ICON (operation);

  cairo_matrix_init_translate (matrix, oper->x, oper->y);
}

static char *
gtk_render_operation_icon_describe (GtkRenderOperation *operation)
{
  GtkRenderOperationIcon *oper = GTK_RENDER_OPERATION_ICON (operation);

  switch (oper->builtin_type)
  {
    default:
      g_warning ("missing builtin type %u", (guint) oper->builtin_type);
      /* fall through */
    case GTK_CSS_IMAGE_BUILTIN_NONE:
      return g_strdup ("CSS icon");
    case GTK_CSS_IMAGE_BUILTIN_CHECK:
      return g_strdup ("CSS check icon (unchecked)");
    case GTK_CSS_IMAGE_BUILTIN_CHECK_CHECKED:
      return g_strdup ("CSS check icon (checked)");
    case GTK_CSS_IMAGE_BUILTIN_CHECK_INCONSISTENT:
      return g_strdup ("CSS check icon (inconsistent)");
    case GTK_CSS_IMAGE_BUILTIN_OPTION:
      return g_strdup ("CSS option icon (unchecked)");
    case GTK_CSS_IMAGE_BUILTIN_OPTION_CHECKED:
      return g_strdup ("CSS option icon (checked)");
    case GTK_CSS_IMAGE_BUILTIN_OPTION_INCONSISTENT:
      return g_strdup ("CSS option icon (inconsistent)");
    case GTK_CSS_IMAGE_BUILTIN_ARROW_UP:
      return g_strdup ("CSS up arrow icon");
    case GTK_CSS_IMAGE_BUILTIN_ARROW_DOWN:
      return g_strdup ("CSS down arrow icon");
    case GTK_CSS_IMAGE_BUILTIN_ARROW_LEFT:
      return g_strdup ("CSS left arrow icon");
    case GTK_CSS_IMAGE_BUILTIN_ARROW_RIGHT:
      return g_strdup ("CSS right arrow icon");
    case GTK_CSS_IMAGE_BUILTIN_EXPANDER_HORIZONTAL_LEFT:
      return g_strdup ("CSS horizontal left expander icon");
    case GTK_CSS_IMAGE_BUILTIN_EXPANDER_VERTICAL_LEFT:
      return g_strdup ("CSS vertical left expander icon");
    case GTK_CSS_IMAGE_BUILTIN_EXPANDER_HORIZONTAL_RIGHT:
      return g_strdup ("CSS horizontal right expander icon");
    case GTK_CSS_IMAGE_BUILTIN_EXPANDER_VERTICAL_RIGHT:
      return g_strdup ("CSS vertical right expander icon");
    case GTK_CSS_IMAGE_BUILTIN_EXPANDER_HORIZONTAL_LEFT_EXPANDED:
      return g_strdup ("CSS horizontal left expander icon (expanded)");
    case GTK_CSS_IMAGE_BUILTIN_EXPANDER_VERTICAL_LEFT_EXPANDED:
      return g_strdup ("CSS vertical left expander icon (expanded)");
    case GTK_CSS_IMAGE_BUILTIN_EXPANDER_HORIZONTAL_RIGHT_EXPANDED:
      return g_strdup ("CSS horizontal right expander icon (expanded)");
    case GTK_CSS_IMAGE_BUILTIN_EXPANDER_VERTICAL_RIGHT_EXPANDED:
      return g_strdup ("CSS vertical right expander icon (expanded)");
    case GTK_CSS_IMAGE_BUILTIN_GRIP_TOPLEFT:
      return g_strdup ("CSS top left grip icon");
    case GTK_CSS_IMAGE_BUILTIN_GRIP_TOP:
      return g_strdup ("CSS top grip icon");
    case GTK_CSS_IMAGE_BUILTIN_GRIP_TOPRIGHT:
      return g_strdup ("CSS top right grip icon");
    case GTK_CSS_IMAGE_BUILTIN_GRIP_RIGHT:
      return g_strdup ("CSS right grip icon");
    case GTK_CSS_IMAGE_BUILTIN_GRIP_BOTTOMRIGHT:
      return g_strdup ("CSS bottom right grip icon");
    case GTK_CSS_IMAGE_BUILTIN_GRIP_BOTTOM:
      return g_strdup ("CSS bottom grip icon");
    case GTK_CSS_IMAGE_BUILTIN_GRIP_BOTTOMLEFT:
      return g_strdup ("CSS bottom left grip icon");
    case GTK_CSS_IMAGE_BUILTIN_GRIP_LEFT:
      return g_strdup ("CSS left grip icon");
    case GTK_CSS_IMAGE_BUILTIN_PANE_SEPARATOR:
      return g_strdup ("CSS pane separator icon");
    case GTK_CSS_IMAGE_BUILTIN_HANDLE:
      return g_strdup ("CSS handle icon");
    case GTK_CSS_IMAGE_BUILTIN_SPINNER:
      return g_strdup ("CSS spinner icon");
  }
}

static void
gtk_render_operation_icon_draw (GtkRenderOperation *operation,
                                  cairo_t            *cr)
{
  GtkRenderOperationIcon *oper = GTK_RENDER_OPERATION_ICON (operation);

  gtk_css_style_render_icon (oper->style,
                             cr,
                             0, 0, 
                             oper->width,
                             oper->height,
                             oper->builtin_type);
}

static void
gtk_render_operation_icon_finalize (GObject *object)
{
  GtkRenderOperationIcon *oper = GTK_RENDER_OPERATION_ICON (object);

  g_object_unref (oper->style);

  G_OBJECT_CLASS (gtk_render_operation_icon_parent_class)->finalize (object);
}

static void
gtk_render_operation_icon_class_init (GtkRenderOperationIconClass *klass)
{
  GtkRenderOperationClass *operation_class = GTK_RENDER_OPERATION_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_render_operation_icon_finalize;

  operation_class->get_clip = gtk_render_operation_icon_get_clip;
  operation_class->get_matrix = gtk_render_operation_icon_get_matrix;
  operation_class->describe = gtk_render_operation_icon_describe;
  operation_class->draw = gtk_render_operation_icon_draw;
}

static void
gtk_render_operation_icon_init (GtkRenderOperationIcon *image)
{
}

GtkRenderOperation *
gtk_render_operation_icon_new (GtkCssStyle            *style,
                               double                  x,
                               double                  y,
                               double                  width,
                               double                  height,
                               GtkCssImageBuiltinType  builtin_type)
{
  GtkRenderOperationIcon *result;

  g_return_val_if_fail (GTK_IS_CSS_STYLE (style), NULL);

  result = g_object_new (GTK_TYPE_RENDER_OPERATION_ICON, NULL);
  
  result->style = g_object_ref (style);
  result->x = x;
  result->y = y;
  result->width = width;
  result->height = height;
  result->builtin_type = builtin_type;
 
  return GTK_RENDER_OPERATION (result);
}

