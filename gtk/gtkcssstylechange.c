/* GTK - The GIMP Toolkit
 * Copyright (C) 2015 Benjamin Otte <otte@gnome.org>
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

#include "gtkcssstylechangeprivate.h"

#include "gtkcssstylepropertyprivate.h"

void
gtk_css_style_change_init (GtkCssStyleChange *change,
                           GtkCssStyle       *old_style,
                           GtkCssStyle       *new_style)
{
  change->old_style = g_object_ref (old_style);
  change->new_style = g_object_ref (new_style);

  change->n_compared = 0;

  change->affects = 0;
  change->changes = _gtk_bitmask_new ();
  
  /* Make sure we don't do extra work if old and new are equal. */
  if (old_style == new_style)
    change->n_compared = GTK_CSS_PROPERTY_N_PROPERTIES;
}

void
gtk_css_style_change_finish (GtkCssStyleChange *change)
{
  g_object_unref (change->old_style);
  g_object_unref (change->new_style);
  _gtk_bitmask_free (change->changes);
}

GtkCssStyle *
gtk_css_style_change_get_old_style (GtkCssStyleChange *change)
{
  return change->old_style;
}

GtkCssStyle *
gtk_css_style_change_get_new_style (GtkCssStyleChange *change)
{
  return change->new_style;
}

static gboolean
gtk_css_style_compare_next_value (GtkCssStyleChange *change)
{
  if (change->n_compared == GTK_CSS_PROPERTY_N_PROPERTIES)
    return FALSE;

  if (!_gtk_css_value_equal (gtk_css_style_get_value (change->old_style, change->n_compared),
                             gtk_css_style_get_value (change->new_style, change->n_compared)))
    {
      change->affects |= _gtk_css_style_property_get_affects (_gtk_css_style_property_lookup_by_id (change->n_compared));
      change->changes = _gtk_bitmask_set (change->changes, change->n_compared, TRUE);
    }

  change->n_compared++;

  return TRUE;
}

gboolean
gtk_css_style_change_has_change (GtkCssStyleChange *change)
{
  do {
    if (!_gtk_bitmask_is_empty (change->changes))
      return TRUE;
  } while (gtk_css_style_compare_next_value (change));

  return FALSE;
}

gboolean
gtk_css_style_change_affects (GtkCssStyleChange *change,
                              GtkCssAffects      affects)
{
  do {
    if (change->affects & affects)
      return TRUE;
  } while (gtk_css_style_compare_next_value (change));

  return FALSE;
}

gboolean
gtk_css_style_change_changes_property (GtkCssStyleChange *change,
                                       guint              id)
{
  while (change->n_compared <= id)
    gtk_css_style_compare_next_value (change);

  return _gtk_bitmask_get (change->changes, id);
}

void
gtk_css_style_change_print (GtkCssStyleChange *change,
                            GString           *string)
{
  int i;
  GtkCssStyle *old = gtk_css_style_change_get_old_style (change);
  GtkCssStyle *new = gtk_css_style_change_get_new_style (change);

  for (i = 0; i < GTK_CSS_PROPERTY_N_PROPERTIES; i ++)
    {
      if (gtk_css_style_change_changes_property (change, i))
        {
          GtkCssStyleProperty *prop;
          GtkCssValue *value;
          const char *name;

          prop = _gtk_css_style_property_lookup_by_id (i);
          name = _gtk_style_property_get_name (GTK_STYLE_PROPERTY (prop));

          value = gtk_css_style_get_value (old, i);
          _gtk_css_value_print (value, string);

          g_string_append_printf (string, "%s: ", name);
          _gtk_css_value_print (value, string);
          g_string_append (string, "\n");

          g_string_append_printf (string, "%s: ", name);
          value = gtk_css_style_get_value (new, i);
          _gtk_css_value_print (value, string);
          g_string_append (string, "\n");
        }
    }

}

char *
gtk_css_style_change_to_string (GtkCssStyleChange *change)
{
  GString *string = g_string_new ("");

  gtk_css_style_change_print (change, string);

  return g_string_free (string, FALSE);
}
