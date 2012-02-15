/*
 * Copyright © 2012 Red Hat Inc.
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include <string.h>
#include <cairo-gobject.h>

#include "gtkcsscomputedvaluesprivate.h"

#include "gtkcssstylepropertyprivate.h"
#include "gtkcsstypesprivate.h"
#include "gtkprivatetypebuiltins.h"
#include "gtkshadowprivate.h"
#include "gtkanimationdescription.h"
#include "gtkcssimageprivate.h"

G_DEFINE_TYPE (GtkCssComputedValues, _gtk_css_computed_values, G_TYPE_OBJECT)


typedef GValue GtkCssValue;

static GHashTable *gtk_css_values;

static guint gtk_css_value_hash (GtkCssValue *css_value);
static gboolean gtk_css_value_equal (GtkCssValue *css_value_a, GtkCssValue *css_value_b);

static GtkCssValue *
gtk_css_value_ref (GtkCssValue *v)
{
  v->data[1].v_int++;
  return v;
}

static void
gtk_css_value_unref (GtkCssValue *v)
{
  if (--v->data[1].v_int == 0)
    {
      g_hash_table_remove (gtk_css_values, v);
      g_value_unset ((GValue *)v);
    }
}

static GtkCssValue *
gtk_css_value_dup_value (const GValue *v)
{
  GtkCssValue *new;

  if (v == NULL || !G_IS_VALUE (v))
    return NULL;

  new = g_hash_table_lookup (gtk_css_values, v);
  if (new)
    return gtk_css_value_ref (new);

  new = g_new0 (GtkCssValue, 1);
  g_value_init ((GValue *)new, G_VALUE_TYPE (v));
  g_value_copy (v, (GValue *)new);
  new->data[1].v_int = 1;

  g_hash_table_insert (gtk_css_values, new, new);

  return new;
}


static GtkCssValue *
gtk_css_value_ref_value (GValue *v)
{

  if (v == NULL || !G_IS_VALUE (v))
    return NULL;

  /* Some magic to detect if the GValue is already a GtkCssValue so we can just ref it */
  if (v->data[1].v_int > 0)
    return gtk_css_value_ref ((GtkCssValue *)v);

  return gtk_css_value_dup_value (v);
}

const GValue *
gtk_css_value_peek (GtkCssValue *v)
{
  return (const GValue *)v;
}

static void
gtk_css_computed_values_dispose (GObject *object)
{
  GtkCssComputedValues *values = GTK_CSS_COMPUTED_VALUES (object);

  if (values->values)
    {
      g_ptr_array_unref (values->values);
      values->values = NULL;
    }
  if (values->sections)
    {
      g_ptr_array_unref (values->sections);
      values->sections = NULL;
    }

  G_OBJECT_CLASS (_gtk_css_computed_values_parent_class)->dispose (object);
}

static gboolean
strv_equal (char **a, char **b)
{
  int i;

  if (a == b)
    return TRUE;

  if (a == NULL || b == NULL)
    return FALSE;

  for (i = 0; a[i] != NULL && b[i] != NULL; i++)
    {
      if (strcmp (a[i], b[i]) != 0)
	return FALSE;
    }
  return a[i] == NULL && b[i] == NULL;
}

static guint
strv_hash (char **v)
{
  int i;
  guint hash;

  if (v == NULL)
    return 0;

  hash = 0;
  for (i = 0; v[i] != NULL; i++)
    hash ^= g_str_hash (v[i]);
  return hash;
}

static gboolean
gtk_css_value_equal (GtkCssValue *css_value_a, GtkCssValue *css_value_b)
{
  GType type;
  const GValue *a = gtk_css_value_peek (css_value_a);
  const GValue *b = gtk_css_value_peek (css_value_b);

  if (a == b)
    return TRUE;

  if (a == NULL || b == NULL)
    return FALSE;

  if (a->g_type != b->g_type)
    return FALSE;

  type = a->g_type;
  if (type == G_TYPE_INT || type == G_TYPE_BOOLEAN)
    {
      return a->data[0].v_int == b->data[0].v_int;
    }
  else if (type == G_TYPE_DOUBLE)
    {
      return a->data[0].v_double == b->data[0].v_double;
    }
  else if (type == G_TYPE_LONG ||
	   G_TYPE_IS_ENUM (type) ||
	   G_TYPE_IS_FLAGS (type))
    {
      return a->data[0].v_long == b->data[0].v_long;
    }
  else if (type == GDK_TYPE_RGBA)
    {
      return gdk_rgba_equal (a->data[0].v_pointer,
			     b->data[0].v_pointer);
    }
  else if (type == G_TYPE_STRV)
    {
      return strv_equal (a->data[0].v_pointer,
			 b->data[0].v_pointer);
    }
  else if (type == GTK_TYPE_CSS_NUMBER)
    {
      return _gtk_css_number_equal (a->data[0].v_pointer,
				    b->data[0].v_pointer);
    }
  else if (type == GTK_TYPE_CSS_BORDER_IMAGE_REPEAT)
    {
      GtkCssBorderImageRepeat *aa = a->data[0].v_pointer;
      GtkCssBorderImageRepeat *bb = b->data[0].v_pointer;

      if (aa == bb)
	return TRUE;
      if (aa == NULL || bb == NULL)
	return FALSE;

      return
	aa->vrepeat == bb->vrepeat &&
	aa->hrepeat == bb->hrepeat;
    }
  else if (type == GTK_TYPE_CSS_BORDER_CORNER_RADIUS)
    {
      GtkCssBorderCornerRadius *aa = a->data[0].v_pointer;
      GtkCssBorderCornerRadius *bb = b->data[0].v_pointer;

      if (aa == bb)
	return TRUE;
      if (aa == NULL || bb == NULL)
	return FALSE;

      return
	_gtk_css_number_equal (&aa->horizontal, &bb->horizontal) &&
	_gtk_css_number_equal (&aa->vertical, &bb->vertical);
    }
  else if (type == GTK_TYPE_BORDER)
    {
      GtkBorder *aa = a->data[0].v_pointer;
      GtkBorder *bb = b->data[0].v_pointer;

      if (aa == bb)
	return TRUE;
      if (aa == NULL || bb == NULL)
	return FALSE;

      return
	aa->left == bb->left &&
	aa->right == bb->right &&
	aa->top == bb->top &&
	aa->bottom == bb->bottom;
    }
  else if (type == GTK_TYPE_CSS_BACKGROUND_SIZE)
    {
      GtkCssBackgroundSize *aa = a->data[0].v_pointer;
      GtkCssBackgroundSize *bb = b->data[0].v_pointer;

      if (aa == bb)
	return TRUE;
      if (aa == NULL || bb == NULL)
	return FALSE;

      return
	_gtk_css_number_equal (&aa->width, &bb->width) &&
	_gtk_css_number_equal (&aa->height, &bb->height) &&
	aa->cover == bb->cover &&
	aa->contain == bb->contain;
    }
  else if (type == GTK_TYPE_SHADOW ||
	   type == G_TYPE_PTR_ARRAY ||
	   type == CAIRO_GOBJECT_TYPE_PATTERN ||
	   type == GTK_TYPE_THEMING_ENGINE ||
	   type == GTK_TYPE_ANIMATION_DESCRIPTION||
	   type == GTK_TYPE_CSS_IMAGE)
    {
      /* These are refcounted, compare by pointer */
      return a->data[0].v_pointer == b->data[0].v_pointer;
    }
  else
    {
      g_error ("Can't handle CSS type %s\n", g_type_name (type));
    }

  return FALSE;
}


static guint
gtk_css_value_hash (GtkCssValue *css_value)
{
  GType type;
  const GValue *v = gtk_css_value_peek (css_value);

  if (v == NULL)
    return 0;

  if (v->g_type == 0)
    return 0;

  type = v->g_type;

  if (type == G_TYPE_INT || type == G_TYPE_BOOLEAN)
    {
      return (guint)v->data[0].v_int;
    }
  else if (type == G_TYPE_DOUBLE)
    {
      return (guint)v->data[0].v_double;
    }
  else if (type == G_TYPE_LONG ||
	   G_TYPE_IS_ENUM (type) ||
	   G_TYPE_IS_FLAGS (type))
    {
      return (guint)v->data[0].v_long;
    }
  else if (type == GDK_TYPE_RGBA)
    {
      return gdk_rgba_hash (v->data[0].v_pointer);
    }
  else if (type == G_TYPE_STRV)
    {
      return strv_hash (v->data[0].v_pointer);
    }
  else if (type == GTK_TYPE_CSS_NUMBER)
    {
      return _gtk_css_number_hash (v->data[0].v_pointer);
    }
  else if (type == GTK_TYPE_CSS_BORDER_IMAGE_REPEAT)
    {
      GtkCssBorderImageRepeat *vv = v->data[0].v_pointer;

      if (vv == NULL)
	return 0;

      return ((guint)vv->vrepeat) ^ ((guint)vv->hrepeat);
    }
  else if (type == GTK_TYPE_CSS_BORDER_CORNER_RADIUS)
    {
      GtkCssBorderCornerRadius *vv = v->data[0].v_pointer;

      if (vv == NULL)
	return 0;

      return
	_gtk_css_number_hash (&vv->horizontal) ^
	_gtk_css_number_hash (&vv->vertical);
    }
  else if (type == GTK_TYPE_BORDER)
    {
      GtkBorder *vv = v->data[0].v_pointer;

      if (vv == NULL)
	return 0;

      return
	((guint)vv->left) ^
	(((guint)vv->right) << 16) ^
	((guint)vv->top) ^
	(((guint)vv->bottom) << 16);
    }
  else if (type == GTK_TYPE_CSS_BACKGROUND_SIZE)
    {
      GtkCssBackgroundSize *vv = v->data[0].v_pointer;

      if (vv == NULL)
	return 0;

      return
	_gtk_css_number_hash (&vv->width) ^
	_gtk_css_number_hash (&vv->height) ^
	(((guint)vv->cover) << 9) ^
	(((guint)vv->contain) << 9);
    }
  else if (type == GTK_TYPE_SHADOW ||
	   type == G_TYPE_PTR_ARRAY ||
	   type == CAIRO_GOBJECT_TYPE_PATTERN ||
	   type == GTK_TYPE_THEMING_ENGINE ||
	   type == GTK_TYPE_ANIMATION_DESCRIPTION||
	   type == GTK_TYPE_CSS_IMAGE)
    {
      /* These are refcounted, compare by pointer */
      return GPOINTER_TO_INT (v->data[0].v_pointer);
    }
  else
    {
      g_error ("Can't handle CSS type %s\n", g_type_name (type));
    }

  return FALSE;
}

static void
_gtk_css_computed_values_class_init (GtkCssComputedValuesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gtk_css_computed_values_dispose;

  gtk_css_values = g_hash_table_new ((GHashFunc)gtk_css_value_hash, (GEqualFunc)gtk_css_value_equal);
}

static void
_gtk_css_computed_values_init (GtkCssComputedValues *computed_values)
{
}

GtkCssComputedValues *
_gtk_css_computed_values_new (void)
{
  return g_object_new (GTK_TYPE_CSS_COMPUTED_VALUES, NULL);
}

static void
maybe_unref_section (gpointer section)
{
  if (section)
    gtk_css_section_unref (section);
}

void
_gtk_css_computed_values_compute_value (GtkCssComputedValues *values,
                                        GtkStyleContext      *context,
                                        guint                 id,
                                        const GValue         *specified,
                                        GtkCssSection        *section)
{
  GtkCssStyleProperty *prop;
  GtkStyleContext *parent;

  g_return_if_fail (GTK_IS_CSS_COMPUTED_VALUES (values));
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (specified == NULL || G_IS_VALUE (specified));

  prop = _gtk_css_style_property_lookup_by_id (id);
  parent = gtk_style_context_get_parent (context);

  if (values->values == NULL)
    values->values = g_ptr_array_new_with_free_func ((GDestroyNotify)gtk_css_value_unref);
  if (id <= values->values->len)
   g_ptr_array_set_size (values->values, id + 1);

  /* http://www.w3.org/TR/css3-cascade/#cascade
   * Then, for every element, the value for each property can be found
   * by following this pseudo-algorithm:
   * 1) Identify all declarations that apply to the element
   */
  if (specified != NULL)
    {
      if (G_VALUE_HOLDS (specified, GTK_TYPE_CSS_SPECIAL_VALUE))
        {
          switch (g_value_get_enum (specified))
            {
            case GTK_CSS_INHERIT:
              /* 3) if the value of the winning declaration is ‘inherit’,
               * the inherited value (see below) becomes the specified value.
               */
              specified = NULL;
              break;
            case GTK_CSS_INITIAL:
              /* if the value of the winning declaration is ‘initial’,
               * the initial value (see below) becomes the specified value.
               */
              specified = _gtk_css_style_property_get_initial_value (prop);
              break;
            default:
              /* This is part of (2) above */
              break;
            }
        }

      /* 2) If the cascading process (described below) yields a winning
       * declaration and the value of the winning declaration is not
       * ‘initial’ or ‘inherit’, the value of the winning declaration
       * becomes the specified value.
       */
    }
  else
    {
      if (_gtk_css_style_property_is_inherit (prop))
        {
          /* 4) if the property is inherited, the inherited value becomes
           * the specified value.
           */
          specified = NULL;
        }
      else
        {
          /* 5) Otherwise, the initial value becomes the specified value.
           */
          specified = _gtk_css_style_property_get_initial_value (prop);
        }
    }

  if (specified == NULL && parent == NULL)
    {
      /* If the ‘inherit’ value is set on the root element, the property is
       * assigned its initial value. */
      specified = _gtk_css_style_property_get_initial_value (prop);
    }


  /* Clear existing value, can't reuse as it may be shared */
  if (g_ptr_array_index (values->values, id) != NULL)
    gtk_css_value_unref (g_ptr_array_index (values->values, id));

  if (specified)
    {
      GValue value = G_VALUE_INIT;
      _gtk_css_style_property_compute_value (prop,
                                             &value,
                                             context,
                                             specified);
      g_ptr_array_index (values->values, id) = gtk_css_value_dup_value (&value);
      g_value_unset (&value);
    }
  else
    {
      const GValue *parent_value;
      parent_value = _gtk_style_context_peek_property (parent,
                                                       _gtk_style_property_get_name (GTK_STYLE_PROPERTY (prop)));
      g_ptr_array_index (values->values, id) = gtk_css_value_ref_value ((GValue *)parent_value);
    }

  if (section)
    {
      if (values->sections == NULL)
        values->sections = g_ptr_array_new_with_free_func (maybe_unref_section);
      if (values->sections->len <= id)
        g_ptr_array_set_size (values->sections, id + 1);

      g_ptr_array_index (values->sections, id) = gtk_css_section_ref (section);
    }
}
                                    
void
_gtk_css_computed_values_set_value (GtkCssComputedValues *values,
                                    guint                 id,
                                    const GValue         *value,
                                    GtkCssSection        *section)
{
  g_return_if_fail (GTK_IS_CSS_COMPUTED_VALUES (values));
  g_return_if_fail (value == NULL || G_IS_VALUE (value));

  if (values->values == NULL)
    values->values = g_ptr_array_new_with_free_func ((GDestroyNotify)gtk_css_value_unref);
  if (id <= values->values->len)
   g_ptr_array_set_size (values->values, id + 1);

  /* Clear existing value, can't reuse as it may be shared */
  if (g_ptr_array_index (values->values, id) != NULL)
    gtk_css_value_unref (g_ptr_array_index (values->values, id));

  g_ptr_array_index (values->values, id) = gtk_css_value_ref_value ((GValue *)value);

  if (section)
    {
      if (values->sections == NULL)
        values->sections = g_ptr_array_new_with_free_func (maybe_unref_section);
      if (values->sections->len <= id)
        g_ptr_array_set_size (values->sections, id + 1);

      g_ptr_array_index (values->sections, id) = gtk_css_section_ref (section);
    }
}

const GValue *
_gtk_css_computed_values_get_value (GtkCssComputedValues *values,
                                    guint                 id)
{
  g_return_val_if_fail (GTK_IS_CSS_COMPUTED_VALUES (values), NULL);

  if (values->values == NULL ||
      id >= values->values->len)
    return NULL;

  return gtk_css_value_peek (g_ptr_array_index (values->values, id));
}

const GValue *
_gtk_css_computed_values_get_value_by_name (GtkCssComputedValues *values,
                                            const char           *name)
{
  GtkStyleProperty *prop;

  g_return_val_if_fail (GTK_IS_CSS_COMPUTED_VALUES (values), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  prop = _gtk_style_property_lookup (name);
  g_assert (GTK_IS_CSS_STYLE_PROPERTY (prop));
  
  return _gtk_css_computed_values_get_value (values, _gtk_css_style_property_get_id (GTK_CSS_STYLE_PROPERTY (prop)));
}

GtkCssSection *
_gtk_css_computed_values_get_section (GtkCssComputedValues *values,
                                      guint                 id)
{
  g_return_val_if_fail (GTK_IS_CSS_COMPUTED_VALUES (values), NULL);

  if (values->sections == NULL ||
      id >= values->sections->len)
    return NULL;

  return g_ptr_array_index (values->sections, id);
}

