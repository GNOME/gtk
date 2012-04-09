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
  GType type;
  union {
    gpointer ptr;
    gint gint;
    guint guint;
    double dbl;
    float flt;
  } u;
};

G_DEFINE_BOXED_TYPE (GtkCssValue, _gtk_css_value, _gtk_css_value_ref, _gtk_css_value_unref)

static GtkCssValue *
_gtk_css_value_new (GType type)
{
  GtkCssValue *value;

  value = g_slice_new0 (GtkCssValue);

  value->ref_count = 1;
  value->type = type;

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
      value = _gtk_css_value_new (type);

      if (g_type_is_a (type, G_TYPE_OBJECT))
	value->u.ptr = g_value_dup_object (g_value);
      else if (g_type_is_a (type, G_TYPE_BOXED))
	value->u.ptr = g_value_dup_boxed (g_value);
      else if (g_type_is_a (type, G_TYPE_INT))
	value->u.gint = g_value_get_int (g_value);
      else if (g_type_is_a (type, G_TYPE_UINT))
	value->u.guint = g_value_get_uint (g_value);
      else if (g_type_is_a (type, G_TYPE_BOOLEAN))
	value->u.gint = g_value_get_boolean (g_value);
      else if (g_type_is_a (type, G_TYPE_ENUM))
	value->u.gint = g_value_get_enum (g_value);
      else if (g_type_is_a (type, G_TYPE_FLAGS))
	value->u.guint = g_value_get_flags (g_value);
      else if (g_type_is_a (type, G_TYPE_STRING))
	value->u.ptr = g_value_dup_string (g_value);
      else if (g_type_is_a (type, G_TYPE_DOUBLE))
	value->u.dbl = g_value_get_double (g_value);
      else if (g_type_is_a (type, G_TYPE_FLOAT))
	value->u.flt = g_value_get_float (g_value);
      else
        {
          value->u.ptr = g_slice_new0 (GValue);
          g_value_init (value->u.ptr, G_VALUE_TYPE (g_value));
          g_value_copy (g_value, value->u.ptr);
        }
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
      value = _gtk_css_value_new (type);

      if (g_type_is_a (type, G_TYPE_OBJECT))
	value->u.ptr = g_value_get_object (g_value);
      else if (g_type_is_a (type, G_TYPE_BOXED))
	value->u.ptr = g_value_get_boxed (g_value);
      else if (g_type_is_a (type, G_TYPE_INT))
	value->u.gint = g_value_get_int (g_value);
      else if (g_type_is_a (type, G_TYPE_UINT))
	value->u.guint = g_value_get_uint (g_value);
      else if (g_type_is_a (type, G_TYPE_BOOLEAN))
	value->u.gint = g_value_get_boolean (g_value);
      else if (g_type_is_a (type, G_TYPE_ENUM))
	value->u.gint = g_value_get_enum (g_value);
      else if (g_type_is_a (type, G_TYPE_FLAGS))
	value->u.guint = g_value_get_flags (g_value);
      else if (g_type_is_a (type, G_TYPE_STRING))
	value->u.ptr = g_value_dup_string (g_value);
      else if (g_type_is_a (type, G_TYPE_DOUBLE))
	value->u.dbl = g_value_get_double (g_value);
      else if (g_type_is_a (type, G_TYPE_FLOAT))
	value->u.flt = g_value_get_float (g_value);
      else
        {
          value->u.ptr = g_slice_new0 (GValue);
          g_value_init (value->u.ptr, G_VALUE_TYPE (g_value));
          g_value_copy (g_value, value->u.ptr);
          g_value_unset (g_value);
        }
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
	  value = _gtk_css_value_new (G_TYPE_INT);
	  value->u.gint = val;
	  singletons[val] = value;
	}
      return _gtk_css_value_ref (singletons[val]);
    }

  value = _gtk_css_value_new (G_TYPE_INT);
  value->u.gint = val;

  return value;
}

GtkCssValue *
_gtk_css_value_new_take_string (char *string)
{
  GtkCssValue *value;

  value = _gtk_css_value_new (G_TYPE_STRING);
  value->u.ptr = string;

  return value;
}

GtkCssValue *
_gtk_css_value_new_from_string (const char *string)
{
  GtkCssValue *value;

  value = _gtk_css_value_new (G_TYPE_STRING);
  value->u.ptr = g_strdup (string);

  return value;
}

static gpointer
g_boxed_copy0 (GType         boxed_type,
	       gconstpointer src_boxed)
{
  if (src_boxed == NULL)
    return NULL;
  return g_boxed_copy (boxed_type, src_boxed);
}

GtkCssValue *
_gtk_css_value_new_from_border (const GtkBorder *border)
{
  GtkCssValue *value;

  value = _gtk_css_value_new (GTK_TYPE_BORDER);
  value->u.ptr = g_boxed_copy0 (GTK_TYPE_BORDER, border);

  return value;
}

GtkCssValue *
_gtk_css_value_new_take_pattern (cairo_pattern_t *v)
{
  GtkCssValue *value;

  value = _gtk_css_value_new (CAIRO_GOBJECT_TYPE_PATTERN);
  value->u.ptr = v;

  return value;
}

GtkCssValue *
_gtk_css_value_new_from_pattern (const cairo_pattern_t *v)
{
  GtkCssValue *value;

  value = _gtk_css_value_new (CAIRO_GOBJECT_TYPE_PATTERN);
  value->u.ptr = g_boxed_copy0 (CAIRO_GOBJECT_TYPE_PATTERN, v);

  return value;
}

GtkCssValue *
_gtk_css_value_new_take_shadow (GtkShadow *v)
{
  GtkCssValue *value;

  value = _gtk_css_value_new (GTK_TYPE_SHADOW);
  value->u.ptr = v;

  return value;
}

GtkCssValue *
_gtk_css_value_new_take_font_description (PangoFontDescription *v)
{
  GtkCssValue *value;

  value = _gtk_css_value_new (PANGO_TYPE_FONT_DESCRIPTION);
  value->u.ptr = v;

  return value;
}

GtkCssValue *
_gtk_css_value_new_take_image (GtkCssImage *v)
{
  GtkCssValue *value;

  value = _gtk_css_value_new (GTK_TYPE_CSS_IMAGE);
  value->u.ptr = v;

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
	  value = _gtk_css_value_new (GTK_TYPE_CSS_NUMBER);
	  value->u.ptr = g_boxed_copy0 (GTK_TYPE_CSS_NUMBER, v);
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
	  value = _gtk_css_value_new (GTK_TYPE_CSS_NUMBER);
	  value->u.ptr = g_boxed_copy0 (GTK_TYPE_CSS_NUMBER, v);
	  px_singletons[i] = value;
	}

      return _gtk_css_value_ref (px_singletons[i]);
    }

  value = _gtk_css_value_new (GTK_TYPE_CSS_NUMBER);
  value->u.ptr = g_boxed_copy0 (GTK_TYPE_CSS_NUMBER, v);

  return value;
}

GtkCssValue *
_gtk_css_value_new_from_rgba (const GdkRGBA *v)
{
  GtkCssValue *value;

  value = _gtk_css_value_new (GDK_TYPE_RGBA);
  value->u.ptr = g_boxed_copy0 (GDK_TYPE_RGBA, v);

  return value;
}

GtkCssValue *
_gtk_css_value_new_from_color (const GdkColor *v)
{
  GtkCssValue *value;

  value = _gtk_css_value_new (GDK_TYPE_COLOR);
  value->u.ptr = g_boxed_copy0 (GDK_TYPE_COLOR, v);

  return value;
}

GtkCssValue *
_gtk_css_value_new_from_background_size (const GtkCssBackgroundSize *v)
{
  GtkCssValue *value;

  value = _gtk_css_value_new (GTK_TYPE_CSS_BACKGROUND_SIZE);
  value->u.ptr = g_boxed_copy0 (GTK_TYPE_CSS_BACKGROUND_SIZE, v);

  return value;
}

GtkCssValue *
_gtk_css_value_new_from_background_position (const GtkCssBackgroundPosition *v)
{
  GtkCssValue *value;

  value = _gtk_css_value_new (GTK_TYPE_CSS_BACKGROUND_POSITION);
  value->u.ptr = g_boxed_copy0 (GTK_TYPE_CSS_BACKGROUND_POSITION, v);

  return value;
}

GtkCssValue *
_gtk_css_value_new_take_symbolic_color (GtkSymbolicColor *v)
{
  GtkCssValue *value;

  value = _gtk_css_value_new (GTK_TYPE_SYMBOLIC_COLOR);
  value->u.ptr = v;

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
  GType type;

  if (value == NULL)
    return;

  if (!g_atomic_int_dec_and_test (&value->ref_count))
    return;

  type = value->type;

  if (g_type_is_a (type, G_TYPE_OBJECT))
    {
      if (value->u.ptr != NULL)
        g_object_unref (value->u.ptr);
    }
  else if (g_type_is_a (type, G_TYPE_BOXED))
    {
      if (value->u.ptr != NULL)
        g_boxed_free (type, value->u.ptr);
    }
  else if (g_type_is_a (type, G_TYPE_STRING))
    g_free (value->u.ptr);
  else if (g_type_is_a (type, G_TYPE_INT))
    {}
  else if (g_type_is_a (type, G_TYPE_UINT))
    {}
  else if (g_type_is_a (type, G_TYPE_BOOLEAN))
    {}
  else if (g_type_is_a (type, G_TYPE_ENUM))
    {}
  else if (g_type_is_a (type, G_TYPE_FLAGS))
    {}
  else if (g_type_is_a (type, G_TYPE_DOUBLE))
    {}
  else if (g_type_is_a (type, G_TYPE_FLOAT))
    {}
  else
    {
      g_value_unset (value->u.ptr);
      g_slice_free (GValue, value->u.ptr);
    }

  g_slice_free (GtkCssValue, value);
}

GType
_gtk_css_value_get_content_type (GtkCssValue *value)
{
  return value->type;
}

gboolean
_gtk_css_value_holds (GtkCssValue *value, GType type)
{
  return g_type_is_a (value->type, type);
}

static void
fill_gvalue (GtkCssValue *value,
	     GValue      *g_value)
{
  GType type;

  type = value->type;

  if (g_type_is_a (type, G_TYPE_OBJECT))
    g_value_set_object (g_value, value->u.ptr);
  else if (g_type_is_a (type, G_TYPE_BOXED))
    g_value_set_boxed (g_value, value->u.ptr);
  else if (g_type_is_a (type, G_TYPE_INT))
    g_value_set_int (g_value, value->u.gint);
  else if (g_type_is_a (type, G_TYPE_UINT))
    g_value_set_uint (g_value, value->u.guint);
  else if (g_type_is_a (type, G_TYPE_BOOLEAN))
    g_value_set_boolean (g_value, value->u.gint);
  else if (g_type_is_a (type, G_TYPE_ENUM))
    g_value_set_enum (g_value, value->u.gint);
  else if (g_type_is_a (type, G_TYPE_FLAGS))
    g_value_set_flags (g_value, value->u.guint);
  else if (g_type_is_a (type, G_TYPE_STRING))
    g_value_set_string (g_value, value->u.ptr);
  else if (g_type_is_a (type, G_TYPE_DOUBLE))
    g_value_set_double (g_value, value->u.dbl);
  else if (g_type_is_a (type, G_TYPE_FLOAT))
    g_value_set_float (g_value, value->u.flt);
  else
    g_value_copy (value->u.ptr, g_value);
}

void
_gtk_css_value_init_gvalue (GtkCssValue *value,
			    GValue      *g_value)
{
  if (value != NULL)
    {
      g_value_init (g_value, value->type);
      fill_gvalue (value, g_value);
    }
}

void
_gtk_css_value_to_gvalue (GtkCssValue *value,
			  GValue      *g_value)
{
  if (value->type == G_VALUE_TYPE (g_value))
    fill_gvalue (value, g_value);
  else if (g_value_type_transformable (value->type, G_VALUE_TYPE (g_value)))
    {
      GValue v = G_VALUE_INIT;
      _gtk_css_value_init_gvalue (value, &v);
      g_value_transform (&v, g_value);
      g_value_unset (&v);
    }
  else
    g_warning ("can't convert css value of type `%s' as value of type `%s'",
	       g_type_name (value->type),
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
  return value->u.gint;
}

GtkCssNumber *
_gtk_css_value_get_number  (GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, GTK_TYPE_CSS_NUMBER), NULL);
  return value->u.ptr;
}

GtkSymbolicColor *
_gtk_css_value_get_symbolic_color  (GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, GTK_TYPE_SYMBOLIC_COLOR), NULL);
  return value->u.ptr;
}

int
_gtk_css_value_get_int (GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, G_TYPE_INT), 0);
  return value->u.gint;
}

double
_gtk_css_value_get_double (GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, G_TYPE_DOUBLE), 0);
  return value->u.dbl;
}

const char *
_gtk_css_value_get_string (GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, G_TYPE_STRING), 0);
  return value->u.ptr;
}

gpointer
_gtk_css_value_dup_object (GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, G_TYPE_OBJECT), NULL);
  if (value->u.ptr)
    return g_object_ref (value->u.ptr);
  return NULL;
}

gpointer
_gtk_css_value_get_object (GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, G_TYPE_OBJECT), NULL);
  return value->u.ptr;
}

gpointer
_gtk_css_value_get_boxed (GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, G_TYPE_BOXED), NULL);
  return value->u.ptr;
}

const char **
_gtk_css_value_get_strv (GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, G_TYPE_STRV), NULL);
  return value->u.ptr;
}

GtkCssImage *
_gtk_css_value_get_image (GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, GTK_TYPE_CSS_IMAGE), NULL);
  return value->u.ptr;
}

GtkBorderStyle
_gtk_css_value_get_border_style (GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, GTK_TYPE_BORDER_STYLE), 0);
  return value->u.gint;
}

GtkCssBackgroundSize *
_gtk_css_value_get_background_size (GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, GTK_TYPE_CSS_BACKGROUND_SIZE), NULL);
  return value->u.ptr;
}

GtkCssBackgroundPosition *
_gtk_css_value_get_background_position (GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, GTK_TYPE_CSS_BACKGROUND_POSITION), NULL);
  return value->u.ptr;
}

GtkCssBorderImageRepeat *
_gtk_css_value_get_border_image_repeat (GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, GTK_TYPE_CSS_BORDER_IMAGE_REPEAT), NULL);
  return value->u.ptr;
}

GtkCssBorderCornerRadius *
_gtk_css_value_get_border_corner_radius (GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, GTK_TYPE_CSS_BORDER_CORNER_RADIUS), NULL);
  return value->u.ptr;
}

PangoFontDescription *
_gtk_css_value_get_font_description (GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, PANGO_TYPE_FONT_DESCRIPTION), 0);
  return value->u.ptr;
}

PangoStyle
_gtk_css_value_get_pango_style (GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, PANGO_TYPE_STYLE), 0);
  return value->u.gint;
}

PangoVariant
_gtk_css_value_get_pango_variant (GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, PANGO_TYPE_VARIANT), 0);
  return value->u.gint;
}

PangoWeight
_gtk_css_value_get_pango_weight (GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, PANGO_TYPE_WEIGHT), 0);
  return value->u.gint;
}

GdkRGBA *
_gtk_css_value_get_rgba (GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, GDK_TYPE_RGBA), NULL);
  return value->u.ptr;
}

cairo_pattern_t *
_gtk_css_value_get_pattern (GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, CAIRO_GOBJECT_TYPE_PATTERN), NULL);
  return value->u.ptr;
}

GtkGradient *
_gtk_css_value_get_gradient (GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, GTK_TYPE_GRADIENT), NULL);
  return value->u.ptr;
}

GtkShadow *
_gtk_css_value_get_shadow (GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, GTK_TYPE_SHADOW), NULL);
  return value->u.ptr;
}
