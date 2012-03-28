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
#include "gtkcssstylefuncsprivate.h"
#include "gtktypebuiltins.h"
#include "gtkgradient.h"
#include <cairo-gobject.h>
#include "gtkprivatetypebuiltins.h"

#include "fallback-c89.c"

struct _GtkCssValue
{
  GTK_CSS_VALUE_BASE
  GType type;
  union {
    gpointer ptr;
    gint gint;
    guint guint;
    double dbl;
    float flt;
  } u;
};

static void
gtk_css_value_default_free (GtkCssValue *value)
{
  GType type = value->type;

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

static gboolean
gtk_css_value_default_equal (const GtkCssValue *value1,
                             const GtkCssValue *value2)
{
  return FALSE;
}

static void
gtk_css_value_default_print (const GtkCssValue *value,
                             GString           *string)
{
  GValue g_value = G_VALUE_INIT;

  _gtk_css_value_init_gvalue (value, &g_value);
  _gtk_css_style_print_value (&g_value, string);
  g_value_unset (&g_value);
}

static const GtkCssValueClass GTK_CSS_VALUE_DEFAULT = {
  gtk_css_value_default_free,
  gtk_css_value_default_equal,
  gtk_css_value_default_print
};

G_DEFINE_BOXED_TYPE (GtkCssValue, _gtk_css_value, _gtk_css_value_ref, _gtk_css_value_unref)

static GtkCssValue *
gtk_css_value_new (GType type)
{
  GtkCssValue *value;

  value = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_DEFAULT);

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
  else
    {
      value = gtk_css_value_new (type);

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
_gtk_css_value_new_from_int (gint val)
{
  GtkCssValue *value;
  static GtkCssValue *singletons[4] = {NULL};

  if (val >= 0 && val < G_N_ELEMENTS (singletons))
    {
      if (singletons[val] == NULL)
	{
	  value = gtk_css_value_new (G_TYPE_INT);
	  value->u.gint = val;
	  singletons[val] = value;
	}
      return _gtk_css_value_ref (singletons[val]);
    }

  value = gtk_css_value_new (G_TYPE_INT);
  value->u.gint = val;

  return value;
}

GtkCssValue *
_gtk_css_value_new_from_enum (GType type,
                              gint  val)
{
  GtkCssValue *value;

  g_return_val_if_fail (g_type_is_a (type, G_TYPE_ENUM), NULL);

  value = gtk_css_value_new (type);
  value->u.gint = val;

  return value;
}

GtkCssValue *
_gtk_css_value_new_from_double (double d)
{
  GtkCssValue *value;

  value = gtk_css_value_new (G_TYPE_DOUBLE);
  value->u.dbl = d;

  return value;
}

GtkCssValue *
_gtk_css_value_new_take_string (char *string)
{
  GtkCssValue *value;

  value = gtk_css_value_new (G_TYPE_STRING);
  value->u.ptr = string;

  return value;
}

GtkCssValue *
_gtk_css_value_new_take_strv (char **strv)
{
  GtkCssValue *value;

  value = gtk_css_value_new (G_TYPE_STRV);
  value->u.ptr = strv;

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
_gtk_css_value_new_from_boxed (GType    type,
                               gpointer boxed)
{
  GtkCssValue *value;

  g_return_val_if_fail (g_type_is_a (type, G_TYPE_BOXED), NULL);

  value = gtk_css_value_new (type);
  value->u.ptr = g_boxed_copy0 (type, boxed);

  return value;
}

GtkCssValue *
_gtk_css_value_new_take_pattern (cairo_pattern_t *v)
{
  GtkCssValue *value;

  value = gtk_css_value_new (CAIRO_GOBJECT_TYPE_PATTERN);
  value->u.ptr = v;

  return value;
}

GtkCssValue *
_gtk_css_value_new_take_image (GtkCssImage *v)
{
  GtkCssValue *value;

  value = gtk_css_value_new (GTK_TYPE_CSS_IMAGE);
  value->u.ptr = v;

  return value;
}

GtkCssValue *
_gtk_css_value_new_from_theming_engine (GtkThemingEngine *v)
{
  GtkCssValue *value;

  value = gtk_css_value_new (GTK_TYPE_THEMING_ENGINE);
  value->u.ptr = g_object_ref (v);

  return value;
}

GtkCssValue *
_gtk_css_value_new_take_binding_sets (GPtrArray *array)
{
  GtkCssValue *value;

  value = gtk_css_value_new (G_TYPE_PTR_ARRAY);
  value->u.ptr = array;

  return value;
}

GtkCssValue *
_gtk_css_value_new_from_color (const GdkColor *v)
{
  GtkCssValue *value;

  value = gtk_css_value_new (GDK_TYPE_COLOR);
  value->u.ptr = g_boxed_copy0 (GDK_TYPE_COLOR, v);

  return value;
}

GtkCssValue *
_gtk_css_value_new_from_background_size (const GtkCssBackgroundSize *v)
{
  GtkCssValue *value;

  value = gtk_css_value_new (GTK_TYPE_CSS_BACKGROUND_SIZE);
  value->u.ptr = g_boxed_copy0 (GTK_TYPE_CSS_BACKGROUND_SIZE, v);

  return value;
}

GtkCssValue *
_gtk_css_value_new_from_background_position (const GtkCssBackgroundPosition *v)
{
  GtkCssValue *value;

  value = gtk_css_value_new (GTK_TYPE_CSS_BACKGROUND_POSITION);
  value->u.ptr = g_boxed_copy0 (GTK_TYPE_CSS_BACKGROUND_POSITION, v);

  return value;
}

GtkCssValue *
_gtk_css_value_new_from_border_corner_radius (const GtkCssBorderCornerRadius *v)
{
  GtkCssValue *value;

  value = gtk_css_value_new (GTK_TYPE_CSS_BORDER_CORNER_RADIUS);
  value->u.ptr = g_boxed_copy0 (GTK_TYPE_CSS_BORDER_CORNER_RADIUS, v);

  return value;
}

GtkCssValue *
_gtk_css_value_new_from_border_image_repeat (const GtkCssBorderImageRepeat *v)
{
  GtkCssValue *value;

  value = gtk_css_value_new (GTK_TYPE_CSS_BORDER_IMAGE_REPEAT);
  value->u.ptr = g_boxed_copy0 (GTK_TYPE_CSS_BORDER_IMAGE_REPEAT, v);

  return value;
}

GtkCssValue *
_gtk_css_value_new_from_border_style (GtkBorderStyle style)
{
  GtkCssValue *value;

  value = gtk_css_value_new (GTK_TYPE_BORDER_STYLE);
  value->u.gint = style;

  return value;
}

GtkCssValue *
_gtk_css_value_new_take_symbolic_color (GtkSymbolicColor *v)
{
  GtkCssValue *value;

  value = gtk_css_value_new (GTK_TYPE_SYMBOLIC_COLOR);
  value->u.ptr = v;

  return value;
}

GtkCssValue *
_gtk_css_value_alloc (const GtkCssValueClass *klass,
                      gsize                   size)
{
  GtkCssValue *value;

  value = g_slice_alloc0 (size);

  value->class = klass;
  value->ref_count = 1;

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

  value->class->free (value);
}

gboolean
_gtk_css_value_equal (const GtkCssValue *value1,
                      const GtkCssValue *value2)
{
  g_return_val_if_fail (value1 != NULL, FALSE);
  g_return_val_if_fail (value2 != NULL, FALSE);

  if (value1->class != value2->class)
    return FALSE;

  return value1->class->equal (value1, value2);
}

void
_gtk_css_value_print (const GtkCssValue *value,
                      GString           *string)
{
  g_return_if_fail (value != NULL);
  g_return_if_fail (string != NULL);

  value->class->print (value, string);
}

GType
_gtk_css_value_get_content_type (const GtkCssValue *value)
{
  g_return_val_if_fail (value->class == &GTK_CSS_VALUE_DEFAULT, G_TYPE_NONE);

  return value->type;
}

gboolean
_gtk_css_value_holds (const GtkCssValue *value, GType type)
{
  g_return_val_if_fail (value->class == &GTK_CSS_VALUE_DEFAULT, FALSE);

  return g_type_is_a (value->type, type);
}

static void
fill_gvalue (const GtkCssValue *value,
	     GValue            *g_value)
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
_gtk_css_value_init_gvalue (const GtkCssValue *value,
			    GValue            *g_value)
{
  if (value != NULL)
    {
      g_return_if_fail (value->class == &GTK_CSS_VALUE_DEFAULT);
      g_value_init (g_value, value->type);
      fill_gvalue (value, g_value);
    }
}

GtkSymbolicColor *
_gtk_css_value_get_symbolic_color (const GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, GTK_TYPE_SYMBOLIC_COLOR), NULL);
  return value->u.ptr;
}

int
_gtk_css_value_get_int (const GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, G_TYPE_INT), 0);
  return value->u.gint;
}

int
_gtk_css_value_get_enum (const GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, G_TYPE_ENUM), 0);
  return value->u.gint;
}

double
_gtk_css_value_get_double (const GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, G_TYPE_DOUBLE), 0);
  return value->u.dbl;
}

const char *
_gtk_css_value_get_string (const GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, G_TYPE_STRING), 0);
  return value->u.ptr;
}

gpointer
_gtk_css_value_dup_object (const GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, G_TYPE_OBJECT), NULL);
  if (value->u.ptr)
    return g_object_ref (value->u.ptr);
  return NULL;
}

gpointer
_gtk_css_value_get_object (const GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, G_TYPE_OBJECT), NULL);
  return value->u.ptr;
}

gpointer
_gtk_css_value_get_boxed (const GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, G_TYPE_BOXED), NULL);
  return value->u.ptr;
}

const char **
_gtk_css_value_get_strv (const GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, G_TYPE_STRV), NULL);
  return value->u.ptr;
}

GtkCssImage *
_gtk_css_value_get_image (const GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, GTK_TYPE_CSS_IMAGE), NULL);
  return value->u.ptr;
}

GtkBorderStyle
_gtk_css_value_get_border_style (const GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, GTK_TYPE_BORDER_STYLE), 0);
  return value->u.gint;
}

const GtkCssBackgroundSize *
_gtk_css_value_get_background_size (const GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, GTK_TYPE_CSS_BACKGROUND_SIZE), NULL);
  return value->u.ptr;
}

const GtkCssBackgroundPosition *
_gtk_css_value_get_background_position (const GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, GTK_TYPE_CSS_BACKGROUND_POSITION), NULL);
  return value->u.ptr;
}

const GtkCssBorderImageRepeat *
_gtk_css_value_get_border_image_repeat (const GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, GTK_TYPE_CSS_BORDER_IMAGE_REPEAT), NULL);
  return value->u.ptr;
}

const GtkCssBorderCornerRadius *
_gtk_css_value_get_border_corner_radius (const GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, GTK_TYPE_CSS_BORDER_CORNER_RADIUS), NULL);
  return value->u.ptr;
}

GtkGradient *
_gtk_css_value_get_gradient (const GtkCssValue *value)
{
  g_return_val_if_fail (_gtk_css_value_holds (value, GTK_TYPE_GRADIENT), NULL);
  return value->u.ptr;
}

