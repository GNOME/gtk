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
#include <errno.h>

#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include "gtkbuilderprivate.h"


typedef struct {
  GtkBuilder *builder;
  GList *classes;
  gboolean packing;
  gboolean packing_started;
  gboolean cell_packing;
  gboolean cell_packing_started;
  gint in_child;
  gint child_started;
  gchar **attribute_names;
  gchar **attribute_values;
  GString *value;
  gboolean unclosed_starttag;
  gint indent;
  char *input_filename;
  char *output_filename;
  FILE *output;
} MyParserData;

static void
canonicalize_key (gchar *key)
{
  gchar *p;

  for (p = key; *p != 0; p++)
    {
      gchar c = *p;

      /* We may meet something like AtkObject::accessible-name */
      if (c == ':' && ((p > key && p[-1] == ':') || p[1] == ':'))
        continue;

      if (c != '-' &&
          (c < '0' || c > '9') &&
          (c < 'A' || c > 'Z') &&
          (c < 'a' || c > 'z'))
        *p = '-';
    }
}

static GParamSpec *
get_property_pspec (MyParserData *data,
                    const gchar  *class_name,
                    const gchar  *property_name)
{
  GType type;
  GObjectClass *class;
  GParamSpec *pspec;
  gchar *canonical_name;

  type = g_type_from_name (class_name);
  if (type == G_TYPE_INVALID)
    {
      GtkBuilder *builder = gtk_builder_new ();
      type = gtk_builder_get_type_from_name (builder, class_name);
      g_object_unref (builder);
      if (type == G_TYPE_INVALID)
        return NULL;
    }

  class = g_type_class_ref (type);
  canonical_name = g_strdup (property_name);
  canonicalize_key (canonical_name);
  if (data->packing)
    pspec = gtk_container_class_find_child_property (class, canonical_name);
  else if (data->cell_packing)
    {
      GObjectClass *cell_class;

      /* We're just assuming that the cell layout is using a GtkCellAreaBox. */
      cell_class = g_type_class_ref (GTK_TYPE_CELL_AREA_BOX);
      pspec = gtk_cell_area_class_find_cell_property (GTK_CELL_AREA_CLASS (cell_class), canonical_name);
      g_type_class_unref (cell_class);
    }
  else
    pspec = g_object_class_find_property (class, canonical_name);
  g_free (canonical_name);
  g_type_class_unref (class);

  return pspec;
}


static gboolean
value_is_default (MyParserData *data,
                  const gchar  *class_name,
                  const gchar  *property_name,
                  const gchar  *value_string)
{
  GValue value = { 0, };
  gboolean ret;
  GError *error = NULL;
  GParamSpec *pspec;

  pspec = get_property_pspec (data, class_name, property_name);

  if (pspec == NULL)
    {
      if (data->packing)
        g_printerr (_("Packing property %s::%s not found\n"), class_name, property_name);
      else if (data->cell_packing)
        g_printerr (_("Cell property %s::%s not found\n"), class_name, property_name);
      else
        g_printerr (_("Property %s::%s not found\n"), class_name, property_name);
      return FALSE;
    }
  else if (g_type_is_a (G_PARAM_SPEC_VALUE_TYPE (pspec), G_TYPE_OBJECT))
    return FALSE;

  if (!gtk_builder_value_from_string (data->builder, pspec, value_string, &value, &error))
    {
      g_printerr (_("Couldn’t parse value for %s::%s: %s\n"), class_name, property_name, error->message);
      g_error_free (error);
      ret = FALSE;
    }
  else
    ret = g_param_value_defaults (pspec, &value);

  g_value_reset (&value);

  return ret;
}

static gboolean
property_is_boolean (MyParserData *data,
                     const gchar  *class_name,
                     const gchar  *property_name)
{
  GParamSpec *pspec;

  pspec = get_property_pspec (data, class_name, property_name);
  if (pspec)
    return G_PARAM_SPEC_VALUE_TYPE (pspec) == G_TYPE_BOOLEAN;

  return FALSE;
}

static const gchar *
canonical_boolean_value (MyParserData *data,
                         const gchar  *string)
{
  GValue value = G_VALUE_INIT;
  gboolean b = FALSE;

  if (gtk_builder_value_from_string_type (data->builder, G_TYPE_BOOLEAN, string, &value, NULL))
    b = g_value_get_boolean (&value);

  return b ? "1" : "0";
}

/* A number of properties unfortunately can't be omitted even
 * if they are nominally set to their default value. In many
 * cases, this is due to subclasses not overriding the default
 * value from the superclass.
 */
static gboolean
needs_explicit_setting (MyParserData *data,
                        const gchar  *class_name,
                        const gchar  *property_name)
{
  struct _Prop {
    const char *class;
    const char *property;
    gboolean packing;
  } props[] = {
    { "GtkAboutDialog", "program-name", 0 },
    { "GtkCalendar", "year", 0 },
    { "GtkCalendar", "month", 0 },
    { "GtkCalendar", "day", 0 },
    { "GtkPlacesSidebar", "show-desktop", 0 },
    { "GtkRadioButton", "draw-indicator", 0 },
    { "GtkGrid", "left-attach", 1 },
    { "GtkGrid", "top-attach", 1 },
    { "GtkWidget", "hexpand", 0 },
    { "GtkWidget", "vexpand", 0 },
    { NULL, NULL, 0 }
  };
  gchar *canonical_name;
  gboolean found;
  gint k;

  canonical_name = g_strdup (property_name);
  g_strdelimit (canonical_name, "_", '-');

  found = FALSE;
  for (k = 0; props[k].class; k++)
    {
      if (strcmp (class_name, props[k].class) == 0 &&
          strcmp (canonical_name, props[k].property) == 0 &&
          data->packing == props[k].packing)
        {
          found = TRUE;
          break;
        }
    }

  g_free (canonical_name);

  return found;
}

static void
maybe_start_packing (MyParserData *data)
{
  if (data->packing)
    {
      if (!data->packing_started)
        {
          g_fprintf (data->output, "%*s<packing>\n", data->indent, "");
          data->indent += 2;
          data->packing_started = TRUE;
        }
    }
}

static void
maybe_start_cell_packing (MyParserData *data)
{
  if (data->cell_packing)
    {
      if (!data->cell_packing_started)
        {
          g_fprintf (data->output, "%*s<cell-packing>\n", data->indent, "");
          data->indent += 2;
          data->cell_packing_started = TRUE;
        }
    }
}

static void
maybe_start_child (MyParserData *data)
{
  if (data->in_child > 0)
    {
      if (data->child_started < data->in_child)
        {
          g_fprintf (data->output, "%*s<child>\n", data->indent, "");
          data->indent += 2;
          data->child_started += 1;
        }
    }
}

static void
maybe_emit_property (MyParserData *data)
{
  gint i;
  gboolean bound;
  gboolean translatable;
  gchar *escaped;
  const gchar *class_name;
  const gchar *property_name;
  const gchar *value_string;

  class_name = (const gchar *)data->classes->data;
  property_name = "";
  value_string = (const gchar *)data->value->str;

  bound = FALSE;
  translatable = FALSE;
  for (i = 0; data->attribute_names[i]; i++)
    {
      if (strcmp (data->attribute_names[i], "bind-source") == 0 ||
          strcmp (data->attribute_names[i], "bind_source") == 0)
        bound = TRUE;
      else if (strcmp (data->attribute_names[i], "translatable") == 0)
        translatable = TRUE;
      else if (strcmp (data->attribute_names[i], "name") == 0)
        property_name = (const gchar *)data->attribute_values[i];
    }

  if (!translatable &&
      !bound &&
      !needs_explicit_setting (data, class_name, property_name))
    {
      for (i = 0; data->attribute_names[i]; i++)
        {
          if (strcmp (data->attribute_names[i], "name") == 0)
            {
              if (data->classes == NULL)
                break;

              if (value_is_default (data, class_name, property_name, value_string))
                return;
            }
        }
    }

  maybe_start_packing (data);
  maybe_start_cell_packing (data);

  g_fprintf (data->output, "%*s<property", data->indent, "");
  for (i = 0; data->attribute_names[i]; i++)
    {
      if (!translatable &&
          (strcmp (data->attribute_names[i], "comments") == 0 ||
           strcmp (data->attribute_names[i], "context") == 0))
        continue;

      escaped = g_markup_escape_text (data->attribute_values[i], -1);

      if (strcmp (data->attribute_names[i], "name") == 0)
        canonicalize_key (escaped);

      g_fprintf (data->output, " %s=\"%s\"", data->attribute_names[i], escaped);
      g_free (escaped);
    }

  if (bound)
    {
      g_fprintf (data->output, "/>\n");
    }
  else
    {
      g_fprintf (data->output, ">");
      if (property_is_boolean (data, class_name, property_name))
        {
          g_fprintf (data->output, "%s", canonical_boolean_value (data, value_string));
        }
      else
        {
          escaped = g_markup_escape_text (value_string, -1);
          g_fprintf (data->output, "%s", escaped);
          g_free (escaped);
        }
      g_fprintf (data->output, "</property>\n");
    }
}

static void
maybe_close_starttag (MyParserData *data)
{
  if (data->unclosed_starttag)
    {
      g_fprintf (data->output, ">\n");
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
  MyParserData *data = user_data;

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
  else if (strcmp (element_name, "cell-packing") == 0)
    {
      data->cell_packing = TRUE;
      data->cell_packing_started = FALSE;

      return;
    }
  else if (strcmp (element_name, "child") == 0)
    {
      data->in_child += 1;

      if (attribute_names[0] == NULL)
        return;

      data->child_started += 1;
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
  else if (strcmp (element_name, "placeholder") == 0)
    {
      return;
    }
  else if (strcmp (element_name, "object") == 0 ||
           strcmp (element_name, "template") == 0)
    {
      maybe_start_child (data);

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

  g_fprintf (data->output, "%*s<%s", data->indent, "", element_name);
  for (i = 0; attribute_names[i]; i++)
    {
      escaped = g_markup_escape_text (attribute_values[i], -1);
      g_fprintf (data->output, " %s=\"%s\"", attribute_names[i], escaped);
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
  MyParserData *data = user_data;

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
  else if (strcmp (element_name, "cell-packing") == 0)
    {
      data->cell_packing = FALSE;
      if (!data->cell_packing_started)
        return;
    }
  else if (strcmp (element_name, "child") == 0)
    {
      data->in_child -= 1;
      if (data->child_started == data->in_child)
        return;
      data->child_started -= 1;
    }
  else if (strcmp (element_name, "placeholder") == 0)
    {
      return;
    }
  else if (strcmp (element_name, "object") == 0 ||
           strcmp (element_name, "template") == 0)
    {
      g_free (data->classes->data);
      data->classes = g_list_delete_link (data->classes, data->classes);
    }

  if (data->value != NULL)
    {
      gchar *escaped;

      if (data->unclosed_starttag)
        g_fprintf (data->output, ">");

      escaped = g_markup_escape_text (data->value->str, -1);
      g_fprintf (data->output, "%s</%s>\n", escaped, element_name);
      g_free (escaped);

      g_string_free (data->value, TRUE);
      data->value = NULL;
    }
  else
    {
      if (data->unclosed_starttag)
        g_fprintf (data->output, "/>\n");
      else
        g_fprintf (data->output, "%*s</%s>\n", data->indent - 2, "", element_name);
    }

  data->indent -= 2;
  data->unclosed_starttag = FALSE;
}

static void
text (GMarkupParseContext  *context,
      const gchar          *text,
      gsize                 text_len,
      gpointer              user_data,
      GError              **error)
{
  MyParserData *data = user_data;

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
  MyParserData *data = user_data;

  maybe_close_starttag (data);

  g_fprintf (data->output, "%*s%s\n", data->indent, "", text);
}

GMarkupParser parser = {
  start_element,
  end_element,
  text,
  passthrough,
  NULL
};

void
do_simplify (int          *argc,
             const char ***argv)
{
  GMarkupParseContext *context;
  gchar *buffer;
  MyParserData data;
  gboolean replace = FALSE;
  char **filenames = NULL;
  GOptionContext *ctx;
  const GOptionEntry entries[] = {
    { "replace", 0, 0, G_OPTION_ARG_NONE, &replace, NULL, NULL },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &filenames, NULL, NULL },
    { NULL, }
  };
  GError *error = NULL;

  ctx = g_option_context_new (NULL);
  g_option_context_set_help_enabled (ctx, FALSE);
  g_option_context_add_main_entries (ctx, entries, NULL);

  if (!g_option_context_parse (ctx, argc, (char ***)argv, &error))
    {
      g_printerr ("%s\n", error->message);
      g_error_free (error);
      exit (1);
    }

  g_option_context_free (ctx);

  if (filenames == NULL)
    {
      g_printerr ("No .ui file specified\n");
      exit (1);
    }

  if (g_strv_length (filenames) > 1)
    {
      g_printerr ("Can only simplify a single .ui file\n");
      exit (1);
    }

  data.input_filename = filenames[0];
  data.output_filename = NULL;

  if (replace)
    {
      int fd;
      fd = g_file_open_tmp ("gtk-builder-tool-XXXXXX", &data.output_filename, NULL);
      data.output = fdopen (fd, "w");
    }
  else
    {
      data.output = stdout;
    }

  if (!g_file_get_contents (filenames[0], &buffer, NULL, &error))
    {
      g_printerr (_("Can’t load file: %s\n"), error->message);
      exit (1);
    }

  data.builder = gtk_builder_new ();
  data.classes = NULL;
  data.attribute_names = NULL;
  data.attribute_values = NULL;
  data.value = NULL;
  data.packing = FALSE;
  data.packing_started = FALSE;
  data.cell_packing = FALSE;
  data.cell_packing_started = FALSE;
  data.in_child = 0;
  data.child_started = 0;
  data.unclosed_starttag = FALSE;
  data.indent = 0;

  context = g_markup_parse_context_new (&parser, G_MARKUP_TREAT_CDATA_AS_TEXT, &data, NULL);
  if (!g_markup_parse_context_parse (context, buffer, -1, &error))
    {
      g_printerr (_("Can’t parse file: %s\n"), error->message);
      exit (1);
    }

  fclose (data.output);

  if (data.output_filename)
    {
      char *content;
      gsize length;

      if (!g_file_get_contents (data.output_filename, &content, &length, &error))
        {
          g_printerr ("Failed to read %s: %s\n", data.output_filename, error->message);
          exit (1);
        }

      if (!g_file_set_contents (data.input_filename, content, length, &error))
        {
          g_printerr ("Failed to write %s: %s\n", data.input_filename, error->message);
          exit (1);
        }
    }
}
