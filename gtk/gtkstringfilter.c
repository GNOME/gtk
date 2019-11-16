/*
 * Copyright © 2019 Benjamin Otte
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

#include "gtkstringfilter.h"

#include "gtkintl.h"
#include "gtktypebuiltins.h"

struct _GtkStringFilter
{
  GtkFilter parent_instance;

  char *search;
  char *search_collated;

  gboolean ignore_case;
  gboolean match_substring;

  GtkExpression *expression;
};

enum {
  PROP_0,
  PROP_EXPRESSION,
  PROP_IGNORE_CASE,
  PROP_MATCH_SUBSTRING,
  PROP_SEARCH,
  NUM_PROPERTIES
};

G_DEFINE_TYPE (GtkStringFilter, gtk_string_filter, GTK_TYPE_FILTER)

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

static char *
gtk_string_filter_collate (GtkStringFilter *self,
                           const char      *s)
{
  char *result, *tmp;

  if (s == NULL || s[0] == '\0')
    return NULL;

  if (!self->ignore_case)
    return g_utf8_collate_key (s, -1);

  tmp = g_utf8_casefold (s, -1);
  result = g_utf8_collate_key (tmp, -1);
  g_free (tmp);

  return result;
}

static gboolean
gtk_string_filter_match (GtkFilter *filter,
                         gpointer   item)
{
  GtkStringFilter *self = GTK_STRING_FILTER (filter);
  GValue value = G_VALUE_INIT;
  char *collated;
  const char *s;
  gboolean result;

  if (self->search_collated == NULL)
    return TRUE;

  if (self->expression == NULL ||
      !gtk_expression_evaluate (self->expression, item, &value))
    return FALSE;
  s = g_value_get_string (&value);
  if (s == NULL)
    return FALSE;
  collated = gtk_string_filter_collate (self, s);

  if (self->match_substring)
    result = strstr (collated, self->search_collated) != NULL;
  else
    result = strcmp (collated, self->search_collated) == 0;

#if 0
  g_print ("%s (%s) %s %s (%s)\n", s, collated, result ? "==" : "!=", self->search, self->search_collated);
#endif

  g_free (collated);
  g_value_unset (&value);

  return result;
}

static GtkFilterMatch
gtk_string_filter_get_strictness (GtkFilter *filter)
{
  GtkStringFilter *self = GTK_STRING_FILTER (filter);

  if (self->search == NULL)
    return GTK_FILTER_MATCH_ALL;

  if (self->expression == NULL)
    return GTK_FILTER_MATCH_NONE;

  return GTK_FILTER_MATCH_SOME;
}

static void
gtk_string_filter_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GtkStringFilter *self = GTK_STRING_FILTER (object);

  switch (prop_id)
    {
    case PROP_EXPRESSION:
      gtk_string_filter_set_expression (self, g_value_get_boxed (value));
      break;

    case PROP_IGNORE_CASE:
      gtk_string_filter_set_ignore_case (self, g_value_get_boolean (value));
      break;

    case PROP_MATCH_SUBSTRING:
      gtk_string_filter_set_match_substring (self, g_value_get_boolean (value));
      break;

    case PROP_SEARCH:
      gtk_string_filter_set_search (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void 
gtk_string_filter_get_property (GObject     *object,
                                guint        prop_id,
                                GValue      *value,
                                GParamSpec  *pspec)
{
  GtkStringFilter *self = GTK_STRING_FILTER (object);

  switch (prop_id)
    {
    case PROP_EXPRESSION:
      g_value_set_boxed (value, self->expression);
      break;

    case PROP_IGNORE_CASE:
      g_value_set_boolean (value, self->ignore_case);
      break;

    case PROP_MATCH_SUBSTRING:
      g_value_set_boolean (value, self->match_substring);
      break;

    case PROP_SEARCH:
      g_value_set_string (value, self->search);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_string_filter_dispose (GObject *object)
{
  GtkStringFilter *self = GTK_STRING_FILTER (object);

  g_clear_pointer (&self->search, g_free);
  g_clear_pointer (&self->search_collated, g_free);
  g_clear_pointer (&self->expression, gtk_expression_unref);

  G_OBJECT_CLASS (gtk_string_filter_parent_class)->dispose (object);
}

static void
gtk_string_filter_class_init (GtkStringFilterClass *class)
{
  GtkFilterClass *filter_class = GTK_FILTER_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  filter_class->match = gtk_string_filter_match;
  filter_class->get_strictness = gtk_string_filter_get_strictness;

  object_class->get_property = gtk_string_filter_get_property;
  object_class->set_property = gtk_string_filter_set_property;
  object_class->dispose = gtk_string_filter_dispose;

  /**
   * GtkStringFilter:expression:
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
   * GtkStringFilter:ignore-case:
   *
   * If matching is case sensitive
   */
  properties[PROP_IGNORE_CASE] =
      g_param_spec_boolean ("ignore-case",
                            P_("Ignore case"),
                            P_("If matching is case sensitive"),
                            TRUE,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkStringFilter:match-substring:
   *
   * If exact matches are necessary or if substrings are allowed
   */
  properties[PROP_MATCH_SUBSTRING] =
      g_param_spec_boolean ("match-substring",
                            P_("Match substring"),
                            P_("If exact matches are necessary or if substrings are allowed"),
                            TRUE,
                            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkStringFilter:search:
   *
   * The search term
   */
  properties[PROP_SEARCH] =
      g_param_spec_string ("search",
                           P_("Search"),
                           P_("The search term"),
                           NULL,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

}

static void
gtk_string_filter_init (GtkStringFilter *self)
{
  self->ignore_case = TRUE;
  self->match_substring = TRUE;

  gtk_filter_changed (GTK_FILTER (self), GTK_FILTER_CHANGE_MATCH_ALL);
}

/**
 * gtk_string_filter_new:
 *
 * Creates a new string filter.
 *
 * You will want to set up the filter by providing a string to search for
 * and by providing a property to look up on the item.
 *
 * Returns: a new #GtkStringFilter
 **/
GtkFilter *
gtk_string_filter_new (void)
{
  return g_object_new (GTK_TYPE_STRING_FILTER, NULL);
}

/**
 * gtk_string_filter_get_search:
 * @self: a #GtkStringFilter
 *
 * Gets the search string set via gtk_string_filter_set_search().
 *
 * Returns: (allow-none) (transfer none): The search string
 **/
const char *
gtk_string_filter_get_search (GtkStringFilter *self)
{
  g_return_val_if_fail (GTK_IS_STRING_FILTER (self), NULL);

  return self->search;
}

/**
 * gtk_string_filter_set_search:
 * @self: a #GtkStringFilter
 * @search: (transfer none) (nullable): The string to search for
 *     or %NULL to clear the search
 *
 * Sets the string to search for.
 **/
void
gtk_string_filter_set_search (GtkStringFilter *self,
                              const char      *search)
{
  GtkFilterChange change;

  g_return_if_fail (GTK_IS_STRING_FILTER (self));

  if (g_strcmp0 (self->search, search) == 0)
    return;

  if (search == NULL || search[0] == 0)
    change = GTK_FILTER_CHANGE_MATCH_ALL;
  else if (self->search == NULL)
    change = GTK_FILTER_CHANGE_MORE_STRICT;
  else if (g_str_has_prefix (search, self->search))
    change = GTK_FILTER_CHANGE_MORE_STRICT;
  else if (g_str_has_prefix (self->search, search))
    change = GTK_FILTER_CHANGE_LESS_STRICT;
  else
    change = GTK_FILTER_CHANGE_DIFFERENT;

  g_free (self->search);
  g_free (self->search_collated);

  self->search = g_strdup (search);
  self->search_collated = gtk_string_filter_collate (self, search);

  gtk_filter_changed (GTK_FILTER (self), change);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SEARCH]);
}

GtkExpression *
gtk_string_filter_get_expression (GtkStringFilter *self)
{
  g_return_val_if_fail (GTK_IS_STRING_FILTER (self), NULL);

  return self->expression;
}

void
gtk_string_filter_set_expression (GtkStringFilter *self,
                                  GtkExpression   *expression)
{
  g_return_if_fail (GTK_IS_STRING_FILTER (self));
  if (expression)
    {
      g_return_if_fail (gtk_expression_get_value_type (expression) == G_TYPE_STRING);
    }

  if (self->expression == expression)
    return;

  g_clear_pointer (&self->expression, gtk_expression_unref);
  self->expression = gtk_expression_ref (expression);

  if (self->search)
    gtk_filter_changed (GTK_FILTER (self), GTK_FILTER_CHANGE_DIFFERENT);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EXPRESSION]);
}

gboolean
gtk_string_filter_get_ignore_case (GtkStringFilter *self)
{
  g_return_val_if_fail (GTK_IS_STRING_FILTER (self), TRUE);

  return self->ignore_case;
}

void
gtk_string_filter_set_ignore_case (GtkStringFilter *self,
                                   gboolean         ignore_case)
{
  g_return_if_fail (GTK_IS_STRING_FILTER (self));

  if (self->ignore_case == ignore_case)
    return;

  self->ignore_case = ignore_case;

  if (self->search)
    {
      g_free (self->search_collated);
      self->search_collated = gtk_string_filter_collate (self, self->search);
      gtk_filter_changed (GTK_FILTER (self), ignore_case ? GTK_FILTER_CHANGE_LESS_STRICT : GTK_FILTER_CHANGE_MORE_STRICT);
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_IGNORE_CASE]);
}

gboolean
gtk_string_filter_get_match_substring (GtkStringFilter *self)
{
  g_return_val_if_fail (GTK_IS_STRING_FILTER (self), TRUE);

  return self->match_substring;
}


void
gtk_string_filter_set_match_substring (GtkStringFilter *self,
                                       gboolean         match_substring)
{
  g_return_if_fail (GTK_IS_STRING_FILTER (self));

  if (self->match_substring == match_substring)
    return;

  self->match_substring = match_substring;

  if (self->search)
    gtk_filter_changed (GTK_FILTER (self), match_substring ? GTK_FILTER_CHANGE_LESS_STRICT : GTK_FILTER_CHANGE_MORE_STRICT);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MATCH_SUBSTRING]);
}

                                                                 
