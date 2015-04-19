/*  Copyright 2015 Red Hat, Inc.
 *
 * GTK+ is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * GLib is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GTK+; see the file COPYING.  If not,
 * see <http://www.gnu.org/licenses/>.
 *
 * Author: Matthias Clasen
 */

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>


typedef struct {
  GtkBuilder *builder;
  GList *classes;
  gboolean packing;
  gboolean packing_started;
  gchar **attribute_names;
  gchar **attribute_values;
  GString *value;
  gboolean unclosed_starttag;
  gint indent;
} ParserData;

static gboolean
value_is_default (ParserData *data,
                  gint        i)
{
  GType type;
  GObjectClass *class;
  GParamSpec *pspec;
  GValue value = { 0, };
  gboolean ret;
  GError *error = NULL;
  const gchar *class_name;
  const gchar *value_string;
  const gchar *property_name;
  gchar *canonical_name;

  class_name = (const gchar *)data->classes->data;
  value_string =(const gchar *)data->value->str;
  property_name = (const gchar *)data->attribute_values[i];

  type = g_type_from_name (class_name);
  if (type == G_TYPE_INVALID)
    return FALSE;

  class = g_type_class_ref (type);
  canonical_name = g_strdup (property_name);
  g_strdelimit (canonical_name, "_", '-');
  if (data->packing)
    pspec = gtk_container_class_find_child_property (class, canonical_name);
  else
    pspec = g_object_class_find_property (class, canonical_name);
  g_free (canonical_name);
  g_type_class_unref (class);

  if (pspec == NULL)
    {
      g_printerr ("P%sroperty %s::%s not found\n", data->packing ? "acking p" : "", class_name, property_name);
      return FALSE;
    }
  else if (g_type_is_a (G_PARAM_SPEC_VALUE_TYPE (pspec), G_TYPE_OBJECT))
    return FALSE;

  if (!gtk_builder_value_from_string (data->builder,
                                      pspec,
                                      value_string,
                                      &value,
                                      &error))
    {
      g_printerr ("Error parsing value: %s\n", error->message);
      g_error_free (error);
      ret = FALSE;
    }
  else
    ret = g_param_value_defaults (pspec, &value);

  g_value_reset (&value);

  return ret;
}

static void
maybe_emit_property (ParserData *data)
{
  gchar *escaped;
  gint i;
  gboolean bound;

  for (i = 0; data->attribute_names[i]; i++)
    {
      if (strcmp (data->attribute_names[i], "name") == 0)
        {
          if (data->classes == NULL)
            break;

          if (value_is_default (data, i))
            return;
        }
    }

  if (data->packing)
    {
      if (!data->packing_started)
        {
          g_print ("%*s<packing>\n", data->indent, "");
          data->indent += 2;
          data->packing_started = TRUE;
        }
    }

  bound = FALSE;

  g_print ("%*s<property", data->indent, "");
  for (i = 0; data->attribute_names[i]; i++)
    {
      if (strcmp (data->attribute_names[i], "bind-source") == 0 ||
          strcmp (data->attribute_names[i], "bind_source") == 0)
        bound = TRUE;

      escaped = g_markup_escape_text (data->attribute_values[i], -1);
      g_print (" %s=\"%s\"", data->attribute_names[i], escaped);
      g_free (escaped);
    }

  if (bound)
    {
      g_print ("/>\n");
    }
  else
    {
      escaped = g_markup_escape_text (data->value->str, -1);
      g_print (">%s</property>\n", escaped);
      g_free (escaped);
    }
}

static void
maybe_close_starttag (ParserData *data)
{
  if (data->unclosed_starttag)
    {
      g_print (">\n");
      data->unclosed_starttag = FALSE;
    }
}

static gboolean
stack_is (GMarkupParseContext *context,
          ...)
{
  va_list args;
  gchar *s, *p;
  const GSList *stack;

  stack = g_markup_parse_context_get_element_stack (context);

  va_start (args, context);
  s = va_arg (args, gchar *);
  while (s)
    {
      if (stack == NULL)
        {
          va_end (args);
          return FALSE;
        }

      p = (gchar *)stack->data;
      if (strcmp (s, p) != 0)
        {
          va_end (args);
          return FALSE;
        }

      s = va_arg (args, gchar *);
      stack = stack->next;
    }

  va_end (args);
  return TRUE;
}

static void
start_element (GMarkupParseContext  *context,
               const gchar          *element_name,
               const gchar         **attribute_names,
               const gchar         **attribute_values,
               gpointer              user_data,
               GError              **error)
{
  gint i;
  gchar *escaped;
  ParserData *data = user_data;

  maybe_close_starttag (data);

  if (strcmp (element_name, "property") == 0)
    {
      g_assert (data->attribute_names == NULL);
      g_assert (data->attribute_values == NULL);
      g_assert (data->value == NULL);

      data->attribute_names = g_strdupv ((gchar **)attribute_names);
      data->attribute_values = g_strdupv ((gchar **)attribute_values);
      data->value = g_string_new ("");

      return;
    }
  else if (strcmp (element_name, "packing") == 0)
    {
      data->packing = TRUE;
      data->packing_started = FALSE;

      return;
    }
  else if (strcmp (element_name, "attribute") == 0)
    {
      /* attribute in label has no content */
      if (data->classes == NULL ||
          strcmp ((gchar *)data->classes->data, "GtkLabel") != 0)
        data->value = g_string_new ("");
    }
  else if (stack_is (context, "item", "items", NULL) ||
           stack_is (context, "action-widget", "action-widgets", NULL) ||
           stack_is (context, "mime-type", "mime-types", NULL) ||
           stack_is (context, "pattern", "patterns", NULL) ||
           stack_is (context, "application", "applications", NULL) ||
           stack_is (context, "col", "row", "data", NULL) ||
           stack_is (context, "mark", "marks", NULL) ||
           stack_is (context, "action", "accessibility", NULL))
    {
      data->value = g_string_new ("");
    }
  else if (strcmp (element_name, "object") == 0 ||
           strcmp (element_name, "template") == 0)
    {
      for (i = 0; attribute_names[i]; i++)
        {
          if (strcmp (attribute_names[i], "class") == 0)
            {
              data->classes = g_list_prepend (data->classes,
                                              g_strdup (attribute_values[i]));
              break;
            }
        }
    }

  g_print ("%*s<%s", data->indent, "", element_name);
  for (i = 0; attribute_names[i]; i++)
    {
      escaped = g_markup_escape_text (attribute_values[i], -1);
      g_print (" %s=\"%s\"", attribute_names[i], escaped);
      g_free (escaped);
    }
  data->unclosed_starttag = TRUE;
  data->indent += 2;
}

static void
end_element (GMarkupParseContext  *context,
             const gchar          *element_name,
             gpointer              user_data,
             GError              **error)
{
  ParserData *data = user_data;

  if (strcmp (element_name, "property") == 0)
    {
      maybe_emit_property (data);

      g_clear_pointer (&data->attribute_names, g_strfreev);
      g_clear_pointer (&data->attribute_values, g_strfreev);
      g_string_free (data->value, TRUE);
      data->value = NULL;
      return;
    }
  else if (strcmp (element_name, "packing") == 0)
    {
      data->packing = FALSE;
      if (!data->packing_started)
        return;
    }
  else if (data->value != 0)
    {
      gchar *escaped;

      escaped = g_markup_escape_text (data->value->str, -1);
      g_print ("%s%s</%s>\n", data->unclosed_starttag ? ">" : "", escaped, element_name);
      g_free (escaped);
      g_string_free (data->value, TRUE);
      data->value = NULL;
      data->unclosed_starttag = FALSE;
      data->indent -= 2;
      return;
    }
  else if (strcmp (element_name, "object") == 0 ||
           strcmp (element_name, "template") == 0)
    {
      g_free (data->classes->data);
      data->classes = g_list_delete_link (data->classes, data->classes);
    }

  data->indent -= 2;

  if (data->unclosed_starttag)
    g_print ("/>\n");
  else
    g_print ("%*s</%s>\n", data->indent, "", element_name);

  data->unclosed_starttag = FALSE;
}

static void
text (GMarkupParseContext  *context,
      const gchar          *text,
      gsize                 text_len,
      gpointer              user_data,
      GError              **error)
{
  ParserData *data = user_data;

  if (data->value)
    {
      g_string_append_len (data->value, text, text_len);
      return;
    }
}

static void
passthrough (GMarkupParseContext  *context,
             const gchar          *text,
             gsize                 text_len,
             gpointer              user_data,
             GError              **error)
{
  ParserData *data = user_data;

  maybe_close_starttag (data);

  g_print ("%*s%s\n", data->indent, "", text);
}

GMarkupParser parser = {
  start_element,
  end_element,
  text,
  passthrough,
  NULL
};

int
main (int argc, char *argv[])
{
  GMarkupParseContext *context;
  GError *error = NULL;
  gchar *buffer;
  ParserData data;

  gtk_init (NULL, NULL);

  gtk_test_register_all_types ();

  if (argc < 2)
    {
      g_printerr ("No file given.\n");
      return 1;
    }

  if (!g_file_get_contents (argv[1], &buffer, NULL, &error))
    {
      g_printerr ("Failed to read file: %s.\n", error->message);
      return 1;
    }

  data.builder = gtk_builder_new ();
  data.classes = NULL;
  data.attribute_names = NULL;
  data.attribute_values = NULL;
  data.value = NULL;
  data.packing = FALSE;
  data.packing_started = FALSE;
  data.unclosed_starttag = FALSE;
  data.indent = 0;

  context = g_markup_parse_context_new (&parser, G_MARKUP_TREAT_CDATA_AS_TEXT, &data, NULL);
  if (!g_markup_parse_context_parse (context, buffer, -1, &error))
    {
      g_printerr ("Failed to parse file: %s.\n", error->message);
      return 1;
    }

  return 0;
}
