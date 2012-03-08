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

#include "gtkcssvalueprivate.h"
#include "gtktypebuiltins.h"
#include "gtkgradient.h"
#include <cairo-gobject.h>
#include "gtkprivatetypebuiltins.h"

#include "fallback-c89.c"

struct _GtkCssValue
{
  volatile gint ref_count;
  GValue g_value;
};

G_DEFINE_BOXED_TYPE (GtkCssValue, _gtk_css_value, _gtk_css_value_ref, _gtk_css_value_unref)

static GtkCssValue *
_gtk_css_value_new (void)
{
  GtkCssValue *value;

  value = g_slice_new0 (GtkCssValue);

  value->ref_count = 1;

  return value;
}

GtkCssValue *
_gtk_css_value_new_from_gvalue (const GValue *g_value)
{
  GtkCssValue *value;
  GType type;

  g_return_val_if_fail (g_value != NULL, NULL);

  type = G_VALUE_TYPE (g_value);

  /* Make sure we reuse the int/number singletons */
  if (type == G_TYPE_INT)
    value = _gtk_css_value_new_from_int (g_value_get_int (g_value));
  else if (type == GTK_TYPE_CSS_NUMBER)
    value = _gtk_css_value_new_from_number (g_value_get_boxed (g_value));
  else
    {
      value = _gtk_css_value_new ();
      g_value_init (&value->g_value, type);
      g_value_copy (g_value, &value->g_value);
    }

  return value;
}

GtkCssValue *
_gtk_css_value_new_take_gvalue (GValue *g_value)
{
  GtkCssValue *value;
  GType type;

  g_return_val_if_fail (g_value != NULL, NULL);

  type = G_VALUE_TYPE (g_value);

  /* Make sure we reuse the int/number singletons */
  if (type == G_TYPE_INT)
    {
      value = _gtk_css_value_new_from_int (g_value_get_int (g_value));
      g_value_unset (g_value);
    }
  else if (type == GTK_TYPE_CSS_NUMBER)
    {
      value = _gtk_css_value_new_from_number (g_value_get_boxed (g_value));
      g_value_unset (g_value);
    }
  else
    {
      value = _gtk_css_value_new ();
      value->g_value = *g_value;
    }

  return value;
}

GtkCssValue *
_gtk_css_value_new_from_int (gint val)
{
  GtkCssValue *value;
  static GtkCssValue *singletons[4] = {NULL};

  if (val >= 0 && val < G_N_ELEMENTS (singletons))
    {
      if (singletons[val] == NULL)
	{
	  value = _gtk_css_value_new ();
	  g_value_init (&value->g_value, G_TYPE_INT);
	  g_value_set_int (&value->g_value, val);
	  singletons[val] = value;
	}
      return _gtk_css_value_ref (singletons[val]);
    }

  value = _gtk_css_value_new ();
  g_value_init (&value->g_value, G_TYPE_INT);
  g_value_set_int (&value->g_value, val);

  return value;
}

GtkCssValue *
_gtk_css_value_new_take_string (char *string)
{
  GtkCssValue *value;

  value = _gtk_css_value_new ();
  g_value_init (&value->g_value, G_TYPE_STRING);
  g_value_take_string (&value->g_value, string);

  return value;
}

GtkCssValue *
_gtk_css_value_new_from_string (const char *string)
{
  GtkCssValue *value;

  value = _gtk_css_value_new ();
  g_value_init (&value->g_value, G_TYPE_STRING);
  g_value_set_string (&value->g_value, string);

  return value;
}

GtkCssValue *
_gtk_css_value_new_from_border (const GtkBorder *border)
{
  GtkCssValue *value;

  value = _gtk_css_value_new ();
  g_value_init (&value->g_value, GTK_TYPE_BORDER);
  g_value_set_boxed (&value->g_value, border);

  return value;
}

GtkCssValue *
_gtk_css_value_new_take_pattern (cairo_pattern_t *v)
{
  GtkCssValue *value;

  value = _gtk_css_value_new ();
  g_value_init (&value->g_value, CAIRO_GOBJECT_TYPE_PATTERN);
  g_value_take_boxed (&value->g_value, v);

  return value;
}

GtkCssValue *
_gtk_css_value_new_from_pattern (const cairo_pattern_t *v)
{
  GtkCssValue *value;

  value = _gtk_css_value_new ();
  g_value_init (&value->g_value, CAIRO_GOBJECT_TYPE_PATTERN);
  g_value_set_boxed (&value->g_value, v);

  return value;
}

GtkCssValue *
_gtk_css_value_new_take_shadow (GtkShadow *v)
{
  GtkCssValue *value;

  value = _gtk_css_value_new ();
  g_value_init (&value->g_value, GTK_TYPE_SHADOW);
  g_value_take_boxed (&value->g_value, v);

  return value;
}

GtkCssValue *
_gtk_css_value_new_take_font_description (PangoFontDescription *v)
{
  GtkCssValue *value;

  value = _gtk_css_value_new ();
  g_value_init (&value->g_value, PANGO_TYPE_FONT_DESCRIPTION);
  g_value_take_boxed (&value->g_value, v);

  return value;
}

GtkCssValue *
_gtk_css_value_new_take_image (GtkCssImage *v)
{
  GtkCssValue *value;

  value = _gtk_css_value_new ();
  g_value_init (&value->g_value, GTK_TYPE_CSS_IMAGE);
  g_value_take_object (&value->g_value, v);

  return value;
}

GtkCssValue *
_gtk_css_value_new_from_number (const GtkCssNumber *v)
{
  GtkCssValue *value;
  static GtkCssValue *zero_singleton = NULL;
  static GtkCssValue *px_singletons[5] = {NULL};

  if (v->unit == GTK_CSS_NUMBER &&
      v->value == 0)
    {
      if (zero_singleton == NULL)
	{
	  value = _gtk_css_value_new ();
	  g_value_init (&value->g_value, GTK_TYPE_CSS_NUMBER);
	  g_value_set_boxed (&value->g_value, v);
	  zero_singleton = value;
	}
      return _gtk_css_value_ref (zero_singleton);
    }

  if (v->unit == GTK_CSS_PX &&
      (v->value == 0 ||
       v->value == 1 ||
       v->value == 2 ||
       v->value == 3 ||
       v->value == 4))
    {
      int i = round (v->value);
      if (px_singletons[i] == NULL)
	{
	  value = _gtk_css_value_new ();
	  g_value_init (&value->g_value, GTK_TYPE_CSS_NUMBER);
	  g_value_set_boxed (&value->g_value, v);
	  px_singletons[i] = value;
	}

      return _gtk_css_value_ref (px_singletons[i]);
    }

  value = _gtk_css_value_new ();
  g_value_init (&value->g_value, GTK_TYPE_CSS_NUMBER);
  g_value_set_boxed (&value->g_value, v);

  return value;
}

GtkCssValue *
_gtk_css_value_new_from_rgba (const GdkRGBA *v)
{
  GtkCssValue *value;

  value = _gtk_css_value_new ();
  g_value_init (&value->g_value, GDK_TYPE_RGBA);
  g_value_set_boxed (&value->g_value, v);

  return value;
}

GtkCssValue *
_gtk_css_value_new_from_color (const GdkColor *v)
{
  GtkCssValue *value;

  value = _gtk_css_value_new ();
  g_value_init (&value->g_value, GDK_TYPE_COLOR);
  g_value_set_boxed (&value->g_value, v);

  return value;
}

GtkCssValue *
_gtk_css_value_new_from_background_size (const GtkCssBackgroundSize *v)
{
  GtkCssValue *value;

  value = _gtk_css_value_new ();
  g_value_init (&value->g_value, GTK_TYPE_CSS_BACKGROUND_SIZE);
  g_value_set_boxed (&value->g_value, v);

  return value;
}

GtkCssValue *
_gtk_css_value_new_take_symbolic_color (GtkSymbolicColor *v)
{
  GtkCssValue *value;

  value = _gtk_css_value_new ();
  g_value_init (&value->g_value, GTK_TYPE_SYMBOLIC_COLOR);
  g_value_take_boxed (&value->g_value, v);

  return value;
}

GtkCssValue *
_gtk_css_value_ref (GtkCssValue *value)
{
  g_return_val_if_fail (value != NULL, NULL);

  g_atomic_int_add (&value->ref_count, 1);

  return value;
}

void
_gtk_css_value_unref (GtkCssValue *value)
{
  if (value == NULL)
    return;

  if (!g_atomic_int_dec_and_test (&value->ref_count))
    return;

  g_value_unset (&value->g_value);
  g_slice_free (GtkCssValue, value);
}

GType
_gtk_css_value_get_content_type (GtkCssValue *value)
{
  return G_VALUE_TYPE (&value->g_value);
}

gboolean
_gtk_css_value_holds (GtkCssValue *value, GType type)
{
  return G_VALUE_HOLDS (&value->g_value, type);
}

void
_gtk_css_value_init_gvalue (GtkCssValue *value,
			    GValue      *g_value)
{
  if (value != NULL)
    {
      g_value_init (g_value, G_VALUE_TYPE (&value->g_value));
      g_value_copy (&value->g_value, g_value);
    }
}

void
_gtk_css_value_to_gvalue (GtkCssValue *value,
			  GValue      *g_value)
{
  if (G_VALUE_TYPE (&value->g_value) == G_VALUE_TYPE (g_value))
    g_value_copy (&value->g_value, g_value);
  else if (g_value_type_transformable (G_VALUE_TYPE (&value->g_value), G_VALUE_TYPE (g_value)))
    g_value_transform (&value->g_value, g_value);
  else
    g_warning ("can't convert css value of type `%s' as value of type `%s'",
	       G_VALUE_TYPE_NAME (&value->g_value),
	       G_VALUE_TYPE_NAME (g_value));
}

gboolean
_gtk_css_value_is_special  (GtkCssValue *value)
{
  return _gtk_css_value_holds (value, GTK_TYPE_CSS_SPECIAL_VALUE);
}

GtkCssSpecialValue
_gtk_css_value_get_special_kind  (GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, GTK_TYPE_CSS_SPECIAL_VALUE), 0);
  return g_value_get_enum (&value->g_value);
}

GtkCssNumber *
_gtk_css_value_get_number  (GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, GTK_TYPE_CSS_NUMBER), NULL);
  return g_value_get_boxed (&value->g_value);
}

GtkSymbolicColor *
_gtk_css_value_get_symbolic_color  (GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, GTK_TYPE_SYMBOLIC_COLOR), NULL);
  return g_value_get_boxed (&value->g_value);
}

int
_gtk_css_value_get_int (GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, G_TYPE_INT), 0);
  return g_value_get_int (&value->g_value);
}

double
_gtk_css_value_get_double (GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, G_TYPE_DOUBLE), 0);
  return g_value_get_double (&value->g_value);
}

const char *
_gtk_css_value_get_string (GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, G_TYPE_STRING), 0);
  return g_value_get_string (&value->g_value);
}

gpointer
_gtk_css_value_dup_object (GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, G_TYPE_OBJECT), NULL);
  return g_value_dup_object (&value->g_value);
}

gpointer
_gtk_css_value_get_object (GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, G_TYPE_OBJECT), NULL);
  return g_value_get_object (&value->g_value);
}

gpointer
_gtk_css_value_get_boxed (GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, G_TYPE_BOXED), NULL);
  return g_value_get_boxed (&value->g_value);
}

const char **
_gtk_css_value_get_strv (GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, G_TYPE_STRV), NULL);
  return g_value_get_boxed (&value->g_value);
}

GtkCssImage *
_gtk_css_value_get_image (GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, GTK_TYPE_CSS_IMAGE), NULL);
  return g_value_get_object (&value->g_value);
}

GtkBorderStyle
_gtk_css_value_get_border_style (GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, GTK_TYPE_BORDER_STYLE), 0);
  return g_value_get_enum (&value->g_value);
}

GtkCssBackgroundSize *
_gtk_css_value_get_background_size (GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, GTK_TYPE_CSS_BACKGROUND_SIZE), NULL);
  return g_value_get_boxed (&value->g_value);
}

GtkCssBorderImageRepeat *
_gtk_css_value_get_border_image_repeat (GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, GTK_TYPE_CSS_BORDER_IMAGE_REPEAT), NULL);
  return g_value_get_boxed (&value->g_value);
}

GtkCssBorderCornerRadius *
_gtk_css_value_get_border_corner_radius (GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, GTK_TYPE_CSS_BORDER_CORNER_RADIUS), NULL);
  return g_value_get_boxed (&value->g_value);
}

PangoFontDescription *
_gtk_css_value_get_font_description (GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, PANGO_TYPE_FONT_DESCRIPTION), 0);
  return g_value_get_boxed (&value->g_value);
}

PangoStyle
_gtk_css_value_get_pango_style (GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, PANGO_TYPE_STYLE), 0);
  return g_value_get_enum (&value->g_value);
}

PangoVariant
_gtk_css_value_get_pango_variant (GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, PANGO_TYPE_VARIANT), 0);
  return g_value_get_enum (&value->g_value);
}

PangoWeight
_gtk_css_value_get_pango_weight (GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, PANGO_TYPE_WEIGHT), 0);
  return g_value_get_enum (&value->g_value);
}

GdkRGBA *
_gtk_css_value_get_rgba (GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, GDK_TYPE_RGBA), NULL);
  return g_value_get_boxed (&value->g_value);
}

cairo_pattern_t *
_gtk_css_value_get_pattern (GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, CAIRO_GOBJECT_TYPE_PATTERN), NULL);
  return g_value_get_boxed (&value->g_value);
}

GtkGradient *
_gtk_css_value_get_gradient (GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, GTK_TYPE_GRADIENT), NULL);
  return g_value_get_boxed (&value->g_value);
}

GtkShadow *
_gtk_css_value_get_shadow (GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, GTK_TYPE_SHADOW), NULL);
  return g_value_get_boxed (&value->g_value);
}
