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

typedef struct Element Element;
struct Element {
  Element *parent;
  char *element_name;
  char **attribute_names;
  char **attribute_values;
  char *data;
  GList *children;
};

static void
free_element (gpointer data)
{
  Element *element = data;
  g_list_free_full (element->children, free_element);
  g_free (element->element_name);
  g_strfreev (element->attribute_names);
  g_strfreev (element->attribute_values);
  g_free (element->data);
  g_free (element);
}

typedef struct {
  Element *root;
  Element *current;
  GString *value;
  GtkBuilder *builder;
  const char *input_filename;
  char *output_filename;
  FILE *output;
  gboolean convert3to4;
} MyParserData;

static void
start_element (GMarkupParseContext  *context,
               const char           *element_name,
               const char          **attribute_names,
               const char          **attribute_values,
               gpointer              user_data,
               GError              **error)
{
  MyParserData *data = user_data;
  Element *elt;

  elt = g_new0 (Element, 1);
  elt->parent = data->current;
  elt->element_name = g_strdup (element_name);
  elt->attribute_names = g_strdupv ((char **)attribute_names);
  elt->attribute_values = g_strdupv ((char **)attribute_values);

  if (data->current)
    data->current->children = g_list_append (data->current->children, elt);
  data->current = elt;

  if (data->root == NULL)
    data->root = elt;

  g_string_truncate (data->value, 0);
}

static void
end_element (GMarkupParseContext  *context,
             const char           *element_name,
             gpointer              user_data,
             GError              **error)
{
  MyParserData *data = user_data;

  data->current->data = g_strdup (data->value->str);

  data->current = data->current->parent;
}

static void
text (GMarkupParseContext  *context,
      const char           *text,
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

static GMarkupParser parser = {
  start_element,
  end_element,
  text,
  NULL,
  NULL
};

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
needs_explicit_setting (GParamSpec *pspec,
                        gboolean    packing)
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
  };
  gboolean found;
  gint k;
  const char *class_name;

  class_name = g_type_name (pspec->owner_type);

  found = FALSE;
  for (k = 0; k < G_N_ELEMENTS (props); k++)
    {
      if (strcmp (class_name, props[k].class) == 0 &&
          strcmp (pspec->name, props[k].property) == 0 &&
          packing == props[k].packing)
        {
          found = TRUE;
          break;
        }
    }

  return found;
}

static gboolean
is_pcdata_element (Element *element)
{
  /* elements that can contain text */
  const char *names[] = {
    "property",
    "attribute",
    "action-widget",
    "pattern",
    "mime-type",
    "col",
    "item",
    NULL,
  };

  if (g_str_equal (element->element_name, "property") &&
      (g_strv_contains ((const char * const *)element->attribute_names, "bind-source") ||
       g_strv_contains ((const char * const *)element->attribute_names, "bind_source")))
    return FALSE;

  if (g_strv_contains (names, element->element_name))
    return TRUE;

  return FALSE;
}

static gboolean
is_container_element (Element *element)
{
  /* elements that just hold a list of things and
   * can be omitted when they have no children
   */
  const char *names[] = {
    "packing",
    "cell-packing",
    "attributes",
    "action-widgets",
    "patterns",
    "mime-types",
    "attributes",
    "row",
    "items",
    NULL
  };

  if (g_strv_contains (names, element->element_name))
    return TRUE;

  return FALSE;
}

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
                    const gchar  *property_name,
                    gboolean      packing,
                    gboolean      cell_packing)
{
  GType type;
  GObjectClass *class;
  GParamSpec *pspec;
  gchar *canonical_name;

  type = g_type_from_name (class_name);
  if (type == G_TYPE_INVALID)
    {
      type = gtk_builder_get_type_from_name (data->builder, class_name);
      if (type == G_TYPE_INVALID)
        return NULL;
    }

  class = g_type_class_ref (type);
  canonical_name = g_strdup (property_name);
  canonicalize_key (canonical_name);
  if (packing)
    pspec = gtk_container_class_find_child_property (class, canonical_name);
  else if (cell_packing)
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
                  GParamSpec   *pspec,
                  const gchar  *value_string)
{
  GValue value = { 0, };
  gboolean ret;
  GError *error = NULL;

  if (g_type_is_a (G_PARAM_SPEC_VALUE_TYPE (pspec), G_TYPE_OBJECT))
    return FALSE;

  if (!gtk_builder_value_from_string (data->builder, pspec, value_string, &value, &error))
    {
      g_printerr (_("%s: Couldn’t parse value for %s: %s\n"), data->input_filename, pspec->name, error->message);
      g_error_free (error);
      ret = FALSE;
    }
  else
    ret = g_param_value_defaults (pspec, &value);

  g_value_reset (&value);

  return ret;
}

static const char *
get_class_name (Element *element)
{
  Element *parent = element->parent;
  int i;

  if (g_str_equal (element->element_name, "object"))
    parent = element;

  if (g_str_equal (parent->element_name, "packing"))
    parent = parent->parent->parent; /* child - object */

  if (g_str_equal (parent->element_name, "object"))
    {
      for (i = 0; parent->attribute_names[i]; i++)
        {
          if (g_str_equal (parent->attribute_names[i], "class"))
            return parent->attribute_values[i];
        }
    }
  else if (g_str_equal (parent->element_name, "template"))
    {
      for (i = 0; parent->attribute_names[i]; i++)
        {
          if (g_str_equal (parent->attribute_names[i], "parent"))
            return parent->attribute_values[i];
        }
    }

  return NULL;
}

static gboolean
property_is_boolean (Element      *element,
                     MyParserData *data)
{
  GParamSpec *pspec;
  gboolean packing = FALSE;
  const char *class_name;
  const char *property_name;
  int i;

  if (g_str_equal (element->parent->element_name, "packing"))
    packing = TRUE;

  class_name = get_class_name (element);
  property_name = "";

  for (i = 0; element->attribute_names[i]; i++)
    {
      if (strcmp (element->attribute_names[i], "name") == 0)
        property_name = (const gchar *)element->attribute_values[i];
    }

  pspec = get_property_pspec (data, class_name, property_name, packing, FALSE);
  if (pspec)
    return G_PARAM_SPEC_VALUE_TYPE (pspec) == G_TYPE_BOOLEAN;

  return FALSE;
}

static gboolean
property_can_be_omitted (Element      *element,
                         MyParserData *data)
{
  gint i;
  gboolean bound;
  gboolean translatable;
  const char *class_name;
  const char *property_name;
  const char *value_string;
  gboolean packing = FALSE;
  gboolean cell_packing = FALSE;
  GParamSpec *pspec;

  if (g_str_equal (element->parent->element_name, "packing"))
    packing = TRUE;
  if (g_str_equal (element->parent->element_name, "cell-packing"))
    cell_packing = TRUE;

  class_name = get_class_name (element);
  property_name = "";
  value_string = element->data;

  bound = FALSE;
  translatable = FALSE;
  for (i = 0; element->attribute_names[i]; i++)
    {
      if (strcmp (element->attribute_names[i], "bind-source") == 0 ||
          strcmp (element->attribute_names[i], "bind_source") == 0)
        bound = TRUE;
      else if (strcmp (element->attribute_names[i], "translatable") == 0)
        translatable = TRUE;
      else if (strcmp (element->attribute_names[i], "name") == 0)
        property_name = (const gchar *)element->attribute_values[i];
    }

  if (translatable)
    return FALSE;

  if (bound)
    return FALSE;

  pspec = get_property_pspec (data, class_name, property_name, packing, cell_packing);

  if (pspec == NULL)
    {
      if (packing)
        g_printerr (_("%s: Packing property %s::%s not found\n"), data->input_filename, class_name, property_name);
      else if (cell_packing)
        g_printerr (_("%s: Cell property %s::%s not found\n"), data->input_filename, class_name, property_name);
      else
        g_printerr (_("%s: Property %s::%s not found\n"), data->input_filename, class_name, property_name);
      return FALSE;
    }

  if (needs_explicit_setting (pspec, packing))
    return FALSE;

  return value_is_default (data, pspec, value_string);
}

static gboolean
property_has_been_removed (Element      *element,
                           MyParserData *data)
{
  const gchar *class_name;
  const gchar *property_name;
  gboolean packing = FALSE;
  struct _Prop {
    const char *class;
    const char *property;
    gboolean packing;
  } props[] = {
    { "GtkActionBar", "position", 1 },
    { "GtkButtonBox", "secondary", 1 },
    { "GtkButtonBox", "non-homogeneous", 1 },
    { "GtkBox", "pack-type", 1 },
    { "GtkBox", "position", 1 },
    { "GtkHeaderBar", "position", 1 },
    { "GtkPopoverMenu", "position", 1 },
    { "GtkMenu", "left-attach", 1 },
    { "GtkMenu", "right-attach", 1 },
    { "GtkMenu", "top-attach", 1 },
    { "GtkMenu", "bottom-attach", 1 }
  };
  gchar *canonical_name;
  gboolean found;
  gint i, k;

  if (g_str_equal (element->parent->element_name, "packing"))
    packing = TRUE;

  class_name = get_class_name (element);
  property_name = "";

  for (i = 0; element->attribute_names[i]; i++)
    {
      if (strcmp (element->attribute_names[i], "name") == 0)
        property_name = (const gchar *)element->attribute_values[i];
    }

  canonical_name = g_strdup (property_name);
  g_strdelimit (canonical_name, "_", '-');

  found = FALSE;
  for (k = 0; k < G_N_ELEMENTS (props); k++)
    {
      if (strcmp (class_name, props[k].class) == 0 &&
          strcmp (canonical_name, props[k].property) == 0 &&
          packing == props[k].packing)
        {
          found = TRUE;
          break;
        }
    }

  g_free (canonical_name);

  return found;
}

static Element *
rewrite_stack_child (Element *child, MyParserData *data)
{
  Element *object = NULL;
  Element *packing = NULL;
  Element *new_object;
  Element *prop;
  GList *l;

  if (!g_str_equal (child->element_name, "child"))
    return child;

  for (l = child->children; l; l = l->next)
    {
      Element *elt = l->data;
      if (g_str_equal (elt->element_name, "object"))
        object = elt;
      else if (g_str_equal (elt->element_name, "packing"))
        packing = elt;
    }

  if (!packing)
    return child;

  new_object = g_new0 (Element, 1);
  new_object->element_name = g_strdup ("object");
  new_object->attribute_names = g_new0 (char *, 2);
  new_object->attribute_names[0] = g_strdup ("class");
  new_object->attribute_values = g_new0 (char *, 2);
  new_object->attribute_values[0] = g_strdup ("GtkStackPage");
  new_object->children = packing->children;
  packing->children = NULL;

  prop = g_new0 (Element, 1);
  prop->element_name = g_strdup ("property");
  prop->attribute_names = g_new0 (char *, 2);
  prop->attribute_names[0] = g_strdup ("name");
  prop->attribute_values = g_new0 (char *, 2);
  prop->attribute_values[0] = g_strdup ("child");
  prop->children = g_list_append (prop->children, object);
  new_object->children = g_list_append (new_object->children, prop);
      
  g_list_free (child->children);
  child->children = g_list_append (NULL, new_object);

  return child;
}

static void
rewrite_stack (Element      *element,
               MyParserData *data)
{
  GList *l, *new_children;

  new_children = NULL;
  for (l = element->children; l; l = l->next)
    {
      Element *child = l->data;
      new_children = g_list_append (new_children, rewrite_stack_child (child, data));
    }

  g_list_free (element->children);
  element->children = new_children;
}

static Element *
rewrite_assistant_child (Element *child, MyParserData *data)
{
  Element *object = NULL;
  Element *packing = NULL;
  Element *new_object;
  Element *prop;
  GList *l;

  if (!g_str_equal (child->element_name, "child"))
    return child;

  for (l = child->children; l; l = l->next)
    {
      Element *elt = l->data;
      if (g_str_equal (elt->element_name, "object"))
        object = elt;
      else if (g_str_equal (elt->element_name, "packing"))
        packing = elt;
    }

  if (!packing)
    return child;

  new_object = g_new0 (Element, 1);
  new_object->element_name = g_strdup ("object");
  new_object->attribute_names = g_new0 (char *, 2);
  new_object->attribute_names[0] = g_strdup ("class");
  new_object->attribute_values = g_new0 (char *, 2);
  new_object->attribute_values[0] = g_strdup ("GtkAssistantPage");
  new_object->children = packing->children;
  packing->children = NULL;

  prop = g_new0 (Element, 1);
  prop->element_name = g_strdup ("property");
  prop->attribute_names = g_new0 (char *, 2);
  prop->attribute_names[0] = g_strdup ("name");
  prop->attribute_values = g_new0 (char *, 2);
  prop->attribute_values[0] = g_strdup ("child");
  prop->children = g_list_append (prop->children, object);
  new_object->children = g_list_append (new_object->children, prop);
      
  g_list_free (child->children);
  child->children = g_list_append (NULL, new_object);

  return child;
}

static void
rewrite_assistant (Element      *element,
                   MyParserData *data)
{
  GList *l, *new_children;

  new_children = NULL;
  for (l = element->children; l; l = l->next)
    {
      Element *child = l->data;
      new_children = g_list_append (new_children, rewrite_assistant_child (child, data));
    }

  g_list_free (element->children);
  element->children = new_children;
}

static gboolean
has_attribute (Element    *elt,
               const char *name,
               const char *value)
{
  int i;

  for (i = 0; elt->attribute_names[i]; i++)
    {
      if (strcmp (elt->attribute_names[i], name) == 0 &&
          strcmp (elt->attribute_values[i], value) == 0)
        return TRUE;
    }

  return FALSE;
}

static Element *
rewrite_notebook_page (Element *child, Element *tab, MyParserData *data)
{
  Element *object = NULL;
  Element *tab_obj = NULL;
  Element *packing = NULL;
  Element *new_object;
  Element *prop;
  GList *l;

  if (!g_str_equal (child->element_name, "child"))
    return child;

  if (has_attribute (child, "type", "tab") ||
      has_attribute (child, "type", "action-start") ||
      has_attribute (child, "type", "action-end"))
    return child;

  for (l = child->children; l; l = l->next)
    {
      Element *elt = l->data;
      if (g_str_equal (elt->element_name, "object"))
        object = elt;
      else if (g_str_equal (elt->element_name, "packing"))
        packing = elt;
    }

  if (!packing && !tab)
    return child;

  if (tab)
    {
      for (l = tab->children; l; l = l->next)
        {
          Element *elt = l->data;
          if (g_str_equal (elt->element_name, "object"))
            tab_obj = elt;
        }
    }

  new_object = g_new0 (Element, 1);
  new_object->element_name = g_strdup ("object");
  new_object->attribute_names = g_new0 (char *, 2);
  new_object->attribute_names[0] = g_strdup ("class");
  new_object->attribute_values = g_new0 (char *, 2);
  new_object->attribute_values[0] = g_strdup ("GtkNotebookPage");
  if (packing)
    {
      new_object->children = packing->children;
      packing->children = NULL;
    }

  prop = g_new0 (Element, 1);
  prop->element_name = g_strdup ("property");
  prop->attribute_names = g_new0 (char *, 2);
  prop->attribute_names[0] = g_strdup ("name");
  prop->attribute_values = g_new0 (char *, 2);
  prop->attribute_values[0] = g_strdup ("child");
  prop->children = g_list_append (prop->children, object);
  new_object->children = g_list_append (new_object->children, prop);

  if (tab_obj)
    {
      prop = g_new0 (Element, 1);
      prop->element_name = g_strdup ("property");
      prop->attribute_names = g_new0 (char *, 2);
      prop->attribute_names[0] = g_strdup ("name");
      prop->attribute_values = g_new0 (char *, 2);
      prop->attribute_values[0] = g_strdup ("tab");
      prop->children = g_list_append (prop->children, tab_obj);
      new_object->children = g_list_append (new_object->children, prop);
    }

  g_list_free (child->children);
  child->children = g_list_append (NULL, new_object);

  return child;
}

static void
rewrite_notebook (Element      *element,
                  MyParserData *data)
{
  GList *l, *new_children;

  new_children = NULL;
  for (l = element->children; l; l = l->next)
    {
      Element *child = l->data;
      Element *tab = l->next ? l->next->data : NULL;

      if (tab && has_attribute (tab, "type", "tab"))
        {
          new_children = g_list_append (new_children, rewrite_notebook_page (child, tab, data));
          l = l->next; /* skip the tab */
        }
      else
        new_children = g_list_append (new_children, rewrite_notebook_page (child, NULL, data));
    }

  g_list_free (element->children);
  element->children = new_children;
}

static gboolean
simplify_element (Element      *element,
                  MyParserData *data)
{
  GList *l;

  if (!is_pcdata_element (element))
    g_clear_pointer (&element->data, g_free);
  else if (g_str_equal (element->element_name, "property") &&
           property_is_boolean (element, data))
    {
      const char *b = canonical_boolean_value (data, element->data);
      g_free (element->data);
      element->data = g_strdup (b);
    }

  l = element->children;
  while (l)
    {
      GList *next = l->next;
      Element *child = l->data;
      if (simplify_element (child, data))
        {
          element->children = g_list_remove (element->children, child);
          free_element (child);
        }
      l = next;
    }

  if (is_container_element (element) && element->children == NULL)
    return TRUE;

  if (g_str_equal (element->element_name, "property") &&
      property_can_be_omitted (element, data))
    return TRUE;

  if (data->convert3to4)
    {
      if (g_str_equal (element->element_name, "object") &&
          g_str_equal (get_class_name (element), "GtkStack"))
        rewrite_stack (element, data);

      if (g_str_equal (element->element_name, "object") &&
          g_str_equal (get_class_name (element), "GtkAssistant"))
        rewrite_assistant (element, data);
          
      if (g_str_equal (element->element_name, "object") &&
          g_str_equal (get_class_name (element), "GtkNotebook"))
        rewrite_notebook (element, data);
          
      if (g_str_equal (element->element_name, "property") &&
          property_has_been_removed (element, data))
        return TRUE;
    }

  return FALSE;
}

static void
simplify_tree (MyParserData *data)
{
  simplify_element (data->root, data);
}

static void
dump_element (Element *element,
              FILE    *output,
              int      indent)
{
  g_fprintf (output, "%*s<%s", indent, "", element->element_name);
  if (element->attribute_names[0])
    {
      int i;
      for (i = 0; element->attribute_names[i]; i++)
        {
          char *escaped = g_markup_escape_text (element->attribute_values[i], -1);
          g_fprintf (output, " %s=\"%s\"", element->attribute_names[i], escaped);
          g_free (escaped);
        }
    }
  if (element->children || element->data)
    {
      g_fprintf (output, ">");

      if (element->children)
        {
          GList *l;

          g_fprintf (output, "\n");
          for (l = element->children; l; l = l->next)
            {
              Element *child = l->data;
              dump_element (child, output, indent + 2);
            }
          g_fprintf (output, "%*s", indent, "");
        }
      else
        {
          char *escaped = g_markup_escape_text (element->data, -1);
          g_fprintf (output, "%s", escaped);
          g_free (escaped);
        }
      g_fprintf (output, "</%s>\n", element->element_name);
    }
  else
    g_fprintf (output, "/>\n"); 
}

static void
dump_tree (MyParserData *data)
{
  dump_element (data->root, data->output, 0);
}

gboolean
simplify_file (const char *filename,
               gboolean    replace,
               gboolean    convert3to4)
{
  GMarkupParseContext *context;
  gchar *buffer;
  MyParserData data;
  GError *error = NULL;

  data.input_filename = filename;
  data.output_filename = NULL;
  data.convert3to4 = convert3to4;

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

  if (!g_file_get_contents (filename, &buffer, NULL, &error))
    {
      g_printerr (_("Can’t load “%s”: %s\n"), filename, error->message);
      return FALSE;
    }

  data.root = NULL;
  data.current = NULL;
  data.value = g_string_new ("");

  context = g_markup_parse_context_new (&parser, G_MARKUP_TREAT_CDATA_AS_TEXT, &data, NULL);
  if (!g_markup_parse_context_parse (context, buffer, -1, &error))
    {
      g_printerr (_("Can’t parse “%s”: %s\n"), filename, error->message);
      return FALSE;
    }

  data.builder = gtk_builder_new ();

  simplify_tree (&data);

  dump_tree (&data);

  fclose (data.output);

  if (data.output_filename)
    {
      char *content;
      gsize length;

      if (!g_file_get_contents (data.output_filename, &content, &length, &error))
        {
          g_printerr (_("Failed to read “%s”: %s\n"), data.output_filename, error->message);
          return FALSE;
        }

      if (!g_file_set_contents (data.input_filename, content, length, &error))
        {
          g_printerr (_("Failed to write %s: “%s”\n"), data.input_filename, error->message);
          return FALSE;
        }
    }

  return TRUE;
}

void
do_simplify (int          *argc,
             const char ***argv)
{
  gboolean replace = FALSE;
  gboolean convert3to4 = FALSE;
  char **filenames = NULL;
  GOptionContext *ctx;
  const GOptionEntry entries[] = {
    { "replace", 0, 0, G_OPTION_ARG_NONE, &replace, NULL, NULL },
    { "3to4", 0, 0, G_OPTION_ARG_NONE, &convert3to4, NULL, NULL },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &filenames, NULL, NULL },
    { NULL, }
  };
  GError *error = NULL;
  int i;

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
      g_printerr (_("No .ui file specified\n"));
      exit (1);
    }

  if (g_strv_length (filenames) > 1 && !replace)
    {
      g_printerr (_("Can only simplify a single .ui file without --replace\n"));
      exit (1);
    }

  for (i = 0; filenames[i]; i++)
    {
      if (!simplify_file (filenames[i], replace, convert3to4))
        exit (1);
    }
}
