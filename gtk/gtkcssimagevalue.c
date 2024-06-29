/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkcssimagevalueprivate.h"

#include "gtkcssimagecrossfadeprivate.h"

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE
  GtkCssImage *image;
};

static void
gtk_css_value_image_free (GtkCssValue *value)
{
  g_object_unref (value->image);
  g_free (value);
}

static GtkCssValue *
gtk_css_value_image_compute (GtkCssValue          *value,
                             guint                 property_id,
                             GtkCssComputeContext *context)
{
  GtkCssImage *image, *computed;
  
  image = _gtk_css_image_value_get_image (value);

  if (image == NULL)
    return gtk_css_value_ref (value);

  computed = _gtk_css_image_compute (image, property_id, context);

  if (computed == image)
    {
      g_object_unref (computed);
      return gtk_css_value_ref (value);
    }

  return _gtk_css_image_value_new (computed);
}

static GtkCssValue *
gtk_css_value_image_resolve (GtkCssValue          *value,
                             GtkCssComputeContext *context,
                             GtkCssValue          *current_color)
{
  if (!gtk_css_value_contains_current_color (value))
    return gtk_css_value_ref (value);

  return _gtk_css_image_value_new (gtk_css_image_resolve (_gtk_css_image_value_get_image (value), context, current_color));
}

static gboolean
gtk_css_value_image_equal (const GtkCssValue *value1,
                           const GtkCssValue *value2)
{
  return _gtk_css_image_equal (value1->image, value2->image);
}

static GtkCssValue *
gtk_css_value_image_transition (GtkCssValue *start,
                                GtkCssValue *end,
                                guint        property_id,
                                double       progress)
{
  GtkCssImage *transition;

  transition = _gtk_css_image_transition (_gtk_css_image_value_get_image (start),
                                          _gtk_css_image_value_get_image (end),
                                          property_id,
                                          progress);
      
  return _gtk_css_image_value_new (transition);
}

static gboolean
gtk_css_value_image_is_dynamic (const GtkCssValue *value)
{
  GtkCssImage *image = _gtk_css_image_value_get_image (value);

  if (image == NULL)
    return FALSE;

  return gtk_css_image_is_dynamic (image);
}

static GtkCssValue *
gtk_css_value_image_get_dynamic_value (GtkCssValue *value,
                                       gint64       monotonic_time)
{
  GtkCssImage *image, *dynamic;
  
  image = _gtk_css_image_value_get_image (value);
  if (image == NULL)
    return gtk_css_value_ref (value);

  dynamic = gtk_css_image_get_dynamic_image (image, monotonic_time);
  if (dynamic == image)
    {
      g_object_unref (dynamic);
      return gtk_css_value_ref (value);
    }

  return _gtk_css_image_value_new (dynamic);
}

static void
gtk_css_value_image_print (const GtkCssValue *value,
                           GString           *string)
{
  if (value->image)
    _gtk_css_image_print (value->image, string);
  else
    g_string_append (string, "none");
}

static const GtkCssValueClass GTK_CSS_VALUE_IMAGE = {
  "GtkCssImageValue",
  gtk_css_value_image_free,
  gtk_css_value_image_compute,
  gtk_css_value_image_resolve,
  gtk_css_value_image_equal,
  gtk_css_value_image_transition,
  gtk_css_value_image_is_dynamic,
  gtk_css_value_image_get_dynamic_value,
  gtk_css_value_image_print
};

GtkCssValue *
_gtk_css_image_value_new (GtkCssImage *image)
{
  static GtkCssValue image_none_singleton = { &GTK_CSS_VALUE_IMAGE, 1, 1, 0, 0, NULL };
  GtkCssValue *value;

  if (image == NULL)
    return gtk_css_value_ref (&image_none_singleton);

  value = gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_IMAGE);
  value->image = image;
  value->is_computed = gtk_css_image_is_computed (image);
  value->contains_current_color = gtk_css_image_contains_current_color (image);

  return value;
}

GtkCssImage *
_gtk_css_image_value_get_image (const GtkCssValue *value)
{
  g_return_val_if_fail (value->class == &GTK_CSS_VALUE_IMAGE, NULL);

  return value->image;
}
