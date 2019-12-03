/*
 * Copyright © 2019 Matthias Clasen
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
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include "gtkstringsorter.h"

#include "gtkintl.h"
#include "gtktypebuiltins.h"

struct _GtkStringSorter
{
  GtkSorter parent_instance;

  gboolean ignore_case;

  GtkExpression *expression;
};

enum {
  PROP_0,
  PROP_EXPRESSION,
  PROP_IGNORE_CASE,
  NUM_PROPERTIES
};

G_DEFINE_TYPE (GtkStringSorter, gtk_string_sorter, GTK_TYPE_SORTER)

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

static int
gtk_string_sorter_compare (GtkSorter *sorter,
                           gpointer   item1,
                           gpointer   item2)
{
  GtkStringSorter *self = GTK_STRING_SORTER (sorter);
  GValue value1 = G_VALUE_INIT;
  GValue value2 = G_VALUE_INIT;
  const char *s1, *s2;
  int result;

  if (self->expression == NULL ||
      !gtk_expression_evaluate (self->expression, item1, &value1) ||
      !gtk_expression_evaluate (self->expression, item2, &value2))
    return FALSE;

  s1 = g_value_get_string (&value1);
  s2 = g_value_get_string (&value2);

  if (s1 == NULL || s2 == NULL)
    return FALSE;

  if (self->ignore_case)
    {
      char *t1, *t2;

      t1 = g_utf8_casefold (s1, -1);
      t2 = g_utf8_casefold (s2, -1);

      result = g_utf8_collate (t1, t2);

      g_free (t1);
      g_free (t2);
    }
  else
    result = g_utf8_collate (s1, s2);

  g_value_unset (&value1);
  g_value_unset (&value2);

  return result;
}

static void
gtk_string_sorter_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GtkStringSorter *self = GTK_STRING_SORTER (object);

  switch (prop_id)
    {
    case PROP_EXPRESSION:
      gtk_string_sorter_set_expression (self, g_value_get_boxed (value));
      break;

    case PROP_IGNORE_CASE:
      gtk_string_sorter_set_ignore_case (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
gtk_string_sorter_get_property (GObject     *object,
                                guint        prop_id,
                                GValue      *value,
                                GParamSpec  *pspec)
{
  GtkStringSorter *self = GTK_STRING_SORTER (object);

  switch (prop_id)
    {
    case PROP_EXPRESSION:
      g_value_set_boxed (value, self->expression);
      break;

    case PROP_IGNORE_CASE:
      g_value_set_boolean (value, self->ignore_case);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_string_sorter_dispose (GObject *object)
{
  GtkStringSorter *self = GTK_STRING_SORTER (object);

  g_clear_pointer (&self->expression, gtk_expression_unref);

  G_OBJECT_CLASS (gtk_string_sorter_parent_class)->dispose (object);
}

static void
gtk_string_sorter_class_init (GtkStringSorterClass *class)
{
  GtkSorterClass *sorter_class = GTK_SORTER_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  sorter_class->compare = gtk_string_sorter_compare;

  object_class->get_property = gtk_string_sorter_get_property;
  object_class->set_property = gtk_string_sorter_set_property;
  object_class->dispose = gtk_string_sorter_dispose;

  /**
   * GtkStringSorter:expression:
   *
   * The expression to evalute on item to get a string to compare with
   */
  properties[PROP_EXPRESSION] =
      g_param_spec_boxed ("expression",
                          P_("Expression"),
                          P_("Expression to compare with"),
                          GTK_TYPE_EXPRESSION,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkStringSorter:ignore-case:
   *
   * If matching is case sensitive
   */
  properties[PROP_IGNORE_CASE] =
      g_param_spec_boolean ("ignore-case",
                            P_("Ignore case"),
                            P_("If matching is case sensitive"),
                            TRUE,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

}

static void
gtk_string_sorter_init (GtkStringSorter *self)
{
  self->ignore_case = TRUE;

  gtk_sorter_changed (GTK_SORTER (self));
}

GtkSorter *
gtk_string_sorter_new (void)
{
  return g_object_new (GTK_TYPE_STRING_SORTER, NULL);
}

GtkExpression *
gtk_string_sorter_get_expression (GtkStringSorter *self)
{
  g_return_val_if_fail (GTK_IS_STRING_SORTER (self), NULL);

  return self->expression;
}

void
gtk_string_sorter_set_expression (GtkStringSorter *self,
                                  GtkExpression   *expression)
{
  g_return_if_fail (GTK_IS_STRING_SORTER (self));
  if (expression)
    g_return_if_fail (gtk_expression_get_value_type (expression) == G_TYPE_STRING);

  if (self->expression == expression)
    return;

  g_clear_pointer (&self->expression, gtk_expression_unref);
  self->expression = gtk_expression_ref (expression);

  gtk_sorter_changed (GTK_SORTER (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EXPRESSION]);
}

gboolean
gtk_string_sorter_get_ignore_case (GtkStringSorter *self)
{
  g_return_val_if_fail (GTK_IS_STRING_SORTER (self), TRUE);

  return self->ignore_case;
}

void
gtk_string_sorter_set_ignore_case (GtkStringSorter *self,
                                   gboolean         ignore_case)
{
  g_return_if_fail (GTK_IS_STRING_SORTER (self));

  if (self->ignore_case == ignore_case)
    return;

  self->ignore_case = ignore_case;

  gtk_sorter_changed (GTK_SORTER (self));

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_IGNORE_CASE]);
}
