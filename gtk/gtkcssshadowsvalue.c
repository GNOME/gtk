/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * Author: Cosimo Cecchi <cosimoc@gnome.org>
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

#include "gtkcssshadowsvalueprivate.h"

#include <math.h>

#include "gtkcssshadowvalueprivate.h"
#include "gtksnapshot.h"

#include <string.h>

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE
  guint         len;
  GtkCssValue  *values[1];
};

static GtkCssValue *    gtk_css_shadows_value_new       (GtkCssValue **values,
                                                         guint         len);

static void
gtk_css_value_shadows_free (GtkCssValue *value)
{
  guint i;

  for (i = 0; i < value->len; i++)
    {
      _gtk_css_value_unref (value->values[i]);
    }

  g_slice_free1 (sizeof (GtkCssValue) + sizeof (GtkCssValue *) * (value->len - 1), value);
}

static GtkCssValue *
gtk_css_value_shadows_compute (GtkCssValue      *value,
                               guint             property_id,
                               GtkStyleProvider *provider,
                               GtkCssStyle      *style,
                               GtkCssStyle      *parent_style)
{
  GtkCssValue *result, *tmp;
  guint i, j;

  if (value->len == 0)
    return _gtk_css_value_ref (value);

  result = NULL;
  for (i = 0; i < value->len; i++)
    {
      tmp = _gtk_css_value_compute (value->values[i], property_id, provider, style, parent_style);

      if (result)
        {
          result->values[i] = tmp;
        }
      else if (tmp != value->values[i])
        {
          result = gtk_css_shadows_value_new (value->values, value->len);
          for (j = 0; j < i; j++)
            {
              _gtk_css_value_ref (result->values[j]);
            }
          result->values[i] = tmp;
        }
      else
        {
          _gtk_css_value_unref (tmp);
        }
    }

  if (result != NULL)
    return result;
  else
    return _gtk_css_value_ref (value);
}

static gboolean
gtk_css_value_shadows_equal (const GtkCssValue *value1,
                             const GtkCssValue *value2)
{
  guint i;

  /* XXX: Should we fill up here? */
  if (value1->len != value2->len)
    return FALSE;

  for (i = 0; i < value1->len; i++)
    {
      if (!_gtk_css_value_equal (value1->values[i],
                                 value2->values[i]))
        return FALSE;
    }

  return TRUE;
}

static GtkCssValue *
gtk_css_value_shadows_transition (GtkCssValue *start,
                                  GtkCssValue *end,
                                  guint        property_id,
                                  double       progress)
{
  guint i, len;
  GtkCssValue **values;

  /* catches the important case of 2 none values */
  if (start == end)
    return _gtk_css_value_ref (start);

  if (start->len > end->len)
    len = start->len;
  else
    len = end->len;

  values = g_newa (GtkCssValue *, len);

  for (i = 0; i < MIN (start->len, end->len); i++)
    {
      values[i] = _gtk_css_value_transition (start->values[i], end->values[i], property_id, progress);
      if (values[i] == NULL)
        {
          while (i--)
            _gtk_css_value_unref (values[i]);
          return NULL;
        }
    }
  if (start->len > end->len)
    {
      for (; i < len; i++)
        {
          GtkCssValue *fill = _gtk_css_shadow_value_new_for_transition (start->values[i]);
          values[i] = _gtk_css_value_transition (start->values[i], fill, property_id, progress);
          _gtk_css_value_unref (fill);

          if (values[i] == NULL)
            {
              while (i--)
                _gtk_css_value_unref (values[i]);
              return NULL;
            }
        }
    }
  else
    {
      for (; i < len; i++)
        {
          GtkCssValue *fill = _gtk_css_shadow_value_new_for_transition (end->values[i]);
          values[i] = _gtk_css_value_transition (fill, end->values[i], property_id, progress);
          _gtk_css_value_unref (fill);

          if (values[i] == NULL)
            {
              while (i--)
                _gtk_css_value_unref (values[i]);
              return NULL;
            }
        }
    }

  return gtk_css_shadows_value_new (values, len);
}

static void
gtk_css_value_shadows_print (const GtkCssValue *value,
                             GString           *string)
{
  guint i;

  if (value->len == 0)
    {
      g_string_append (string, "none");
      return;
    }

  for (i = 0; i < value->len; i++)
    {
      if (i > 0)
        g_string_append (string, ", ");
      _gtk_css_value_print (value->values[i], string);
    }
}

static const GtkCssValueClass GTK_CSS_VALUE_SHADOWS = {
  gtk_css_value_shadows_free,
  gtk_css_value_shadows_compute,
  gtk_css_value_shadows_equal,
  gtk_css_value_shadows_transition,
  NULL,
  NULL,
  gtk_css_value_shadows_print
};

static GtkCssValue none_singleton = { &GTK_CSS_VALUE_SHADOWS, 1, 0, { NULL } };

GtkCssValue *
_gtk_css_shadows_value_new_none (void)
{
  return _gtk_css_value_ref (&none_singleton);
}

static GtkCssValue *
gtk_css_shadows_value_new (GtkCssValue **values,
                           guint         len)
{
  GtkCssValue *result;
           
  g_return_val_if_fail (values != NULL, NULL);
  g_return_val_if_fail (len > 0, NULL);
         
  result = _gtk_css_value_alloc (&GTK_CSS_VALUE_SHADOWS, sizeof (GtkCssValue) + sizeof (GtkCssValue *) * (len - 1));
  result->len = len;
  memcpy (&result->values[0], values, sizeof (GtkCssValue *) * len);
            
  return result;
}

GtkCssValue *
_gtk_css_shadows_value_parse (GtkCssParser *parser,
                              gboolean      box_shadow_mode)
{
  GtkCssValue *value, *result;
  GPtrArray *values;

  if (gtk_css_parser_try_ident (parser, "none"))
    return _gtk_css_shadows_value_new_none ();

  values = g_ptr_array_new ();

  do {
    value = _gtk_css_shadow_value_parse (parser, box_shadow_mode);

    if (value == NULL)
      {
        g_ptr_array_set_free_func (values, (GDestroyNotify) _gtk_css_value_unref);
        g_ptr_array_free (values, TRUE);
        return NULL;
      }

    g_ptr_array_add (values, value);
  } while (gtk_css_parser_try_token (parser, GTK_CSS_TOKEN_COMMA));

  result = gtk_css_shadows_value_new ((GtkCssValue **) values->pdata, values->len);
  g_ptr_array_free (values, TRUE);
  return result;
}

gboolean
_gtk_css_shadows_value_is_none (const GtkCssValue *shadows)
{
  g_return_val_if_fail (shadows->class == &GTK_CSS_VALUE_SHADOWS, TRUE);

  return shadows->len == 0;
}

gsize
gtk_css_shadows_value_get_n_shadows (const GtkCssValue *shadows)
{
  return shadows->len;
}

void
gtk_css_shadows_value_get_shadows (const GtkCssValue  *shadows,
                                   GskShadow          *out_shadows)
{
  guint i;

  for (i = 0; i < shadows->len; i++)
    gtk_css_shadow_value_get_shadow (shadows->values[i], &out_shadows[i]);
}

void
gtk_css_shadows_value_snapshot_outset (const GtkCssValue   *shadows,
                                       GtkSnapshot         *snapshot,
                                       const GskRoundedRect*border_box)
{
  guint i;

  g_return_if_fail (shadows->class == &GTK_CSS_VALUE_SHADOWS);

  for (i = 0; i < shadows->len; i++)
    {
      if (_gtk_css_shadow_value_get_inset (shadows->values[i]))
        continue;

      gtk_css_shadow_value_snapshot_outset (shadows->values[i], snapshot, border_box);
    }
}

void
gtk_css_shadows_value_snapshot_inset (const GtkCssValue   *shadows,
                                      GtkSnapshot         *snapshot,
                                      const GskRoundedRect*padding_box)
{
  guint i;

  g_return_if_fail (shadows->class == &GTK_CSS_VALUE_SHADOWS);

  for (i = 0; i < shadows->len; i++)
    {
      if (!_gtk_css_shadow_value_get_inset (shadows->values[i]))
        continue;

      gtk_css_shadow_value_snapshot_inset (shadows->values[i], snapshot, padding_box);
    }
}

void
_gtk_css_shadows_value_get_extents (const GtkCssValue *shadows,
                                    GtkBorder         *border)
{
  guint i;
  GtkBorder b = { 0 }, sb;
  const GtkCssValue *shadow;

  g_return_if_fail (shadows->class == &GTK_CSS_VALUE_SHADOWS);

  for (i = 0; i < shadows->len; i++)
    {
      shadow = shadows->values[i];

      if (_gtk_css_shadow_value_get_inset (shadow))
        continue;

      gtk_css_shadow_value_get_extents (shadow, &sb);

      b.top = MAX (b.top, sb.top);
      b.right = MAX (b.right, sb.right);
      b.bottom = MAX (b.bottom, sb.bottom);
      b.left = MAX (b.left, sb.left);
    }

  *border = b;
}

gboolean
gtk_css_shadows_value_push_snapshot (const GtkCssValue *value,
                                     GtkSnapshot       *snapshot)
{
  gboolean need_shadow = FALSE;
  int i;

  for (i = 0; i < value->len; i++)
    {
      if (!gtk_css_shadow_value_is_clear (value->values[i]))
        {
          need_shadow = TRUE;
          break;
        }
    }

  if (need_shadow)
    {
      GskShadow *shadows = g_newa (GskShadow, value->len);
      gtk_css_shadows_value_get_shadows (value, shadows);
      gtk_snapshot_push_shadow (snapshot, shadows, value->len);
    }

  return need_shadow;
}
