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

#include <glib/gi18n.h>
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
} MyParserData;

static gboolean
value_is_default (MyParserData *data,
                  gint          i)
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
      g_printerr (_("Couldn't parse value: %s\n"), error->message);
      g_error_free (error);
      ret = FALSE;
    }
  else
    ret = g_param_value_defaults (pspec, &value);

  g_value_reset (&value);

  return ret;
}

/* A number of properties unfortunately can't be omitted even
 * if they are nominally set to their default value. In many
 * cases, this is due to subclasses not overriding the default
 * value from the superclass.
 */
static gboolean
needs_explicit_setting (MyParserData *data,
                        gint          i)
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
    { NULL, NULL, 0 }
  };
  const gchar *class_name;
  const gchar *property_name;
  gchar *canonical_name;
  gboolean found;
  gint k;

  class_name = (const gchar *)data->classes->data;
  property_name = (const gchar *)data->attribute_values[i];
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
          g_print ("%*s<packing>\n", data->indent, "");
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
          g_print ("%*s<cell-packing>\n", data->indent, "");
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
          g_print ("%*s<child>\n", data->indent, "");
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

  bound = FALSE;
  translatable = FALSE;
  for (i = 0; data->attribute_names[i]; i++)
    {
      if (strcmp (data->attribute_names[i], "bind-source") == 0 ||
          strcmp (data->attribute_names[i], "bind_source") == 0)
        bound = TRUE;
      else if (strcmp (data->attribute_names[i], "translatable") == 0)
        translatable = TRUE;
    }

  if (!translatable)
    {
      for (i = 0; data->attribute_names[i]; i++)
        {
          if (strcmp (data->attribute_names[i], "name") == 0)
            {
              if (data->classes == NULL)
                break;

              if (needs_explicit_setting (data, i))
                break;

              if (value_is_default (data, i))
                return;
            }
        }
    }

  maybe_start_packing (data);
  maybe_start_cell_packing (data);

  g_print ("%*s<property", data->indent, "");
  for (i = 0; data->attribute_names[i]; i++)
    {
      if (!translatable &&
          (strcmp (data->attribute_names[i], "comments") == 0 ||
           strcmp (data->attribute_names[i], "context") == 0))
        continue;

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
maybe_close_starttag (MyParserData *data)
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
        g_print (">");

      escaped = g_markup_escape_text (data->value->str, -1);
      g_print ("%s</%s>\n", escaped, element_name);
      g_free (escaped);

      g_string_free (data->value, TRUE);
      data->value = NULL;
    }
  else
    {
      if (data->unclosed_starttag)
        g_print ("/>\n");
      else
        g_print ("%*s</%s>\n", data->indent - 2, "", element_name);
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

  g_print ("%*s%s\n", data->indent, "", text);
}

GMarkupParser parser = {
  start_element,
  end_element,
  text,
  passthrough,
  NULL
};

static void
do_simplify (const gchar *filename)
{
  GMarkupParseContext *context;
  GError *error = NULL;
  gchar *buffer;
  MyParserData data;

  if (!g_file_get_contents (filename, &buffer, NULL, &error))
    {
      g_printerr (_("Can't load file: %s\n"), error->message);
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
      g_printerr (_("Can't parse file: %s\n"), error->message);
      exit (1);
    }
}

static GType
make_fake_type (const gchar *type_name,
                const gchar *parent_name)
{
  GType parent_type;
  GTypeQuery query;

  parent_type = g_type_from_name (parent_name);
  if (parent_type == G_TYPE_INVALID)
    {
      g_printerr ("Failed to lookup template parent type %s\n", parent_name);
      exit (1);
    }

  g_type_query (parent_type, &query);
  return g_type_register_static_simple (parent_type,
                                        type_name,
                                        query.class_size,
                                        NULL,
                                        query.instance_size,
                                        NULL,
                                        0);
}

static void
do_validate_template (const gchar *filename,
                      const gchar *type_name,
                      const gchar *parent_name)
{
  GType template_type;
  GtkWidget *widget;
  GtkBuilder *builder;
  GError *error = NULL;
  gint ret;

  /* Only make a fake type if it doesn't exist yet.
   * This lets us e.g. validate the GtkFileChooserWidget template.
   */
  template_type = g_type_from_name (type_name);
  if (template_type == G_TYPE_INVALID)
    template_type = make_fake_type (type_name, parent_name);

  widget = g_object_new (template_type, NULL);
  if (!widget)
    {
      g_printerr ("Failed to create an instance of the template type %s\n", type_name);
      exit (1);
    }

  builder = gtk_builder_new ();
  ret = gtk_builder_extend_with_template (builder, widget, template_type, " ", 1, &error);
  if (ret)
    ret = gtk_builder_add_from_file (builder, filename, &error);
  g_object_unref (builder);

  if (ret == 0)
    {
      g_printerr ("%s\n", error->message);
      exit (1);
    }
}

static gboolean
parse_template_error (const gchar  *message,
                      gchar       **class_name,
                      gchar       **parent_name)
{
  gchar *p;

  if (!strstr (message, "Not expecting to handle a template"))
    return FALSE;

  p = strstr (message, "(class '");
  if (p)
    {
      *class_name = g_strdup (p + strlen ("(class '"));
      p = strstr (*class_name, "'");
      if (p)
        *p = '\0';
    }
  p = strstr (message, ", parent '");
  if (p)
    {
      *parent_name = g_strdup (p + strlen (", parent '"));
      p = strstr (*parent_name, "'");
      if (p)
        *p = '\0';
    }

  return TRUE;
}

static void
do_validate (const gchar *filename)
{
  GtkBuilder *builder;
  GError *error = NULL;
  gint ret;
  gchar *class_name = NULL;
  gchar *parent_name = NULL;

  builder = gtk_builder_new ();
  ret = gtk_builder_add_from_file (builder, filename, &error);
  g_object_unref (builder);

  if (ret == 0)
    {
      if (g_error_matches (error, GTK_BUILDER_ERROR, GTK_BUILDER_ERROR_UNHANDLED_TAG)  &&
          parse_template_error (error->message, &class_name, &parent_name))
        {
          do_validate_template (filename, class_name, parent_name);
        }
      else
        {
          g_printerr ("%s\n", error->message);
          exit (1);
        }
    }
}

static const gchar *
object_get_name (GObject *object)
{
  if (GTK_IS_BUILDABLE (object))
    return gtk_buildable_get_name (GTK_BUILDABLE (object));
  else
    return g_object_get_data (object, "gtk-builder-name");
}

static void
do_enumerate (const gchar *filename)
{
  GtkBuilder *builder;
  GError *error = NULL;
  gint ret;
  GSList *list, *l;
  GObject *object;
  const gchar *name;

  builder = gtk_builder_new ();
  ret = gtk_builder_add_from_file (builder, filename, &error);

  if (ret == 0)
    {
      g_printerr ("%s\n", error->message);
      exit (1);
    }

  list = gtk_builder_get_objects (builder);
  for (l = list; l; l = l->next)
    {
      object = l->data;
      name = object_get_name (object);
      if (g_str_has_prefix (name, "___") && g_str_has_suffix (name, "___"))
        continue;

      g_print ("%s (%s)\n", name, g_type_name_from_instance ((GTypeInstance*)object));
    }
  g_slist_free (list);

  g_object_unref (builder);
}

static void
usage (void)
{
  g_print (_("Usage:\n"
             "  gtk-builder-tool [COMMAND] FILE\n"
             "\n"
             "Commands:\n"
             "  validate    Validate the file\n"
             "  simplify    Simplify the file\n"
             "  enumerate   List all named objects\n"
             "\n"
             "Perform various tasks on GtkBuilder .ui files.\n"));
  exit (1);
}

int
main (int argc, char *argv[])
{
  g_set_prgname ("gtk-builder-tool");

  gtk_init (NULL, NULL);

  gtk_test_register_all_types ();

  if (argc < 3)
    usage ();

  if (strcmp (argv[1], "validate") == 0)
    do_validate (argv[2]);
  else if (strcmp (argv[1], "simplify") == 0)
    do_simplify (argv[2]);
  else if (strcmp (argv[1], "enumerate") == 0)
    do_enumerate (argv[2]);
  else
    usage ();

  return 0;
}
