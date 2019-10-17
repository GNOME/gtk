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

typedef enum {
  PROP_KIND_OBJECT,
  PROP_KIND_PACKING,
  PROP_KIND_CELL_PACKING,
  PROP_KIND_LAYOUT
} PropKind;

static PropKind
get_prop_kind (Element *element)
{
  g_assert (g_str_equal (element->element_name, "property"));

  if (g_str_equal (element->parent->element_name, "packing"))
    return PROP_KIND_PACKING;
  else if (g_str_equal (element->parent->element_name, "layout"))
    return PROP_KIND_LAYOUT;
  else if (g_str_equal (element->parent->element_name, "cell-packing"))
    return PROP_KIND_CELL_PACKING;
  else
    return PROP_KIND_OBJECT;
}

/* A number of properties unfortunately can't be omitted even
 * if they are nominally set to their default value. In many
 * cases, this is due to subclasses not overriding the default
 * value from the superclass.
 */
static gboolean
needs_explicit_setting (GParamSpec *pspec,
                        PropKind    kind)
{
  struct _Prop {
    const char *class;
    const char *property;
    PropKind kind;
  } props[] = {
    { "GtkAboutDialog", "program-name", PROP_KIND_OBJECT },
    { "GtkCalendar", "year", PROP_KIND_OBJECT },
    { "GtkCalendar", "month", PROP_KIND_OBJECT },
    { "GtkCalendar", "day", PROP_KIND_OBJECT },
    { "GtkPlacesSidebar", "show-desktop", PROP_KIND_OBJECT },
    { "GtkRadioButton", "draw-indicator", PROP_KIND_OBJECT },
    { "GtkWidget", "hexpand", PROP_KIND_OBJECT },
    { "GtkWidget", "vexpand", PROP_KIND_OBJECT },
    { "GtkGrid", "top-attach", PROP_KIND_LAYOUT },
    { "GtkGrid", "left-attach", PROP_KIND_LAYOUT },
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
          kind == props[k].kind)
        {
          found = TRUE;
          break;
        }
    }

  return found;
}

static gboolean
keep_for_rewrite (const char *class_name,
                  const char *property_name,
                  PropKind kind)
{
  struct _Prop {
    const char *class;
    const char *property;
    PropKind kind;
  } props[] = {
    { "GtkPopover", "modal", PROP_KIND_OBJECT },
    { "GtkActionBar", "pack-type", PROP_KIND_PACKING },
    { "GtkHeaderBar", "pack-type", PROP_KIND_PACKING },
    { "GtkPopoverMenu", "submenu", PROP_KIND_PACKING },
    { "GtkToolbar", "expand", PROP_KIND_PACKING },
    { "GtkToolbar", "homogeneous", PROP_KIND_PACKING },
    { "GtkPaned", "resize", PROP_KIND_PACKING },
    { "GtkPaned", "shrink", PROP_KIND_PACKING },
    { "GtkOverlay", "measure", PROP_KIND_PACKING },
    { "GtkOverlay", "clip-overlay", PROP_KIND_PACKING },
    { "GtkGrid", "left-attach", PROP_KIND_PACKING },
    { "GtkGrid", "top-attach", PROP_KIND_PACKING },
    { "GtkGrid", "width", PROP_KIND_PACKING },
    { "GtkGrid", "height", PROP_KIND_PACKING },
    { "GtkStack", "name", PROP_KIND_PACKING },
    { "GtkStack", "title", PROP_KIND_PACKING },
    { "GtkStack", "icon-name", PROP_KIND_PACKING },
    { "GtkStack", "needs-attention", PROP_KIND_PACKING },
  };
  gboolean found;
  gint k;
  char *canonical_name;

  canonical_name = g_strdup (property_name);
  g_strdelimit (canonical_name, "_", '-');

  found = FALSE;
  for (k = 0; k < G_N_ELEMENTS (props); k++)
    {
      if (strcmp (class_name, props[k].class) == 0 &&
          strcmp (canonical_name, props[k].property) == 0 &&
          kind == props[k].kind)
        {
          found = TRUE;
          break;
        }
    }

  g_free (canonical_name);

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
    "mark",
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
    "layout",
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

static struct {
  const char *class;
  const char *layout_manager;
} layout_managers[] = {
  { "GtkBox", "GtkBoxLayout" },
  { "GtkGrid", "GtkGridLayout" },
  { "GtkFixed", "GtkFixedLayout" },
  { "GtkFileChooserButton", "GtkBinLayout" },
  { "GtkFileChooserWidget", "GtkBinLayout" },
  { "GtkOverlay", "GtkOverlayLayout" }
};

static GParamSpec *
get_property_pspec (MyParserData *data,
                    const gchar  *class_name,
                    const gchar  *property_name,
                    PropKind      kind)
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
  switch (kind)
    {
    case PROP_KIND_OBJECT:
      pspec = g_object_class_find_property (class, canonical_name);
      break;

    case PROP_KIND_PACKING:
      pspec = NULL;
      break;

    case PROP_KIND_CELL_PACKING:
      {
        GObjectClass *cell_class;

        /* We're just assuming that the cell layout is using a GtkCellAreaBox. */
        cell_class = g_type_class_ref (GTK_TYPE_CELL_AREA_BOX);
        pspec = gtk_cell_area_class_find_cell_property (GTK_CELL_AREA_CLASS (cell_class), canonical_name);
        g_type_class_unref (cell_class);
      }
      break;

    case PROP_KIND_LAYOUT:
      {
        int i;
        const char *layout_manager = NULL;

        pspec = NULL;

        for (i = 0; i < G_N_ELEMENTS (layout_managers); i++)
          {
            if (g_str_equal (layout_managers[i].class, class_name))
              {
                layout_manager = layout_managers[i].layout_manager;
                break;
              }
          }

        if (layout_manager)
          {
            GtkLayoutManagerClass *layout_manager_class;

            layout_manager_class = GTK_LAYOUT_MANAGER_CLASS (g_type_class_ref (g_type_from_name (layout_manager)));
            if (layout_manager_class->layout_child_type != G_TYPE_INVALID)
              {
                GObjectClass *layout_child_class;
                layout_child_class = g_type_class_ref (layout_manager_class->layout_child_type);
                pspec = g_object_class_find_property (layout_child_class, canonical_name);
                g_type_class_unref (layout_child_class);
              }
            g_type_class_unref (layout_manager_class);
          }
      }
      break;

    default:
      g_assert_not_reached ();
    }
  g_free (canonical_name);
  g_type_class_unref (class);

  return pspec;
}

static const char *get_class_name (Element *element);

static gboolean
value_is_default (Element      *element,
                  MyParserData *data,
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
    {
      /* GtkWidget::visible has a 'smart' default */
      if (pspec->owner_type == GTK_TYPE_WIDGET &&
          g_str_equal (pspec->name, "visible"))
        {
          const char *class_name = get_class_name (element);
          GType type = g_type_from_name (class_name);
          gboolean default_value;

          if (g_type_is_a (type, GTK_TYPE_ROOT) ||
              g_type_is_a (type, GTK_TYPE_POPOVER))
            default_value = FALSE;
          else
            default_value = TRUE;

          ret = g_value_get_boolean (&value) == default_value;
        }
      else
        ret = g_param_value_defaults (pspec, &value);
    }

  g_value_reset (&value);

  return ret;
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
          (value == NULL || strcmp (elt->attribute_values[i], value) == 0))
        return TRUE;
    }

  return FALSE;
}

static const char *
get_attribute_value (Element *element,
                     const char *name)
{
  int i;

  for (i = 0; element->attribute_names[i]; i++)
    {
      if (g_str_equal (element->attribute_names[i], name))
        return element->attribute_values[i];
    }

  return NULL;
}

static void
set_attribute_value (Element *element,
                     const char *name,
                     const char *value)
{
  int i;

  for (i = 0; element->attribute_names[i]; i++)
    {
      if (g_str_equal (element->attribute_names[i], name))
        {
          g_free (element->attribute_values[i]);
          element->attribute_values[i] = g_strdup (value);
          return;
        }
    }
}

static gboolean
element_is_object_or_template (Element *element)
{
  return g_str_equal (element->element_name, "object") ||
         g_str_equal (element->element_name, "template");
}

static const char *
get_class_name (Element *element)
{
  Element *parent = element->parent;

  if (element_is_object_or_template (element))
    parent = element;

  if (g_str_equal (parent->element_name, "packing"))
    parent = parent->parent->parent; /* child - object */
  else if (g_str_equal (parent->element_name, "layout"))
    parent = parent->parent->parent->parent; /* object - child - object */


  if (g_str_equal (parent->element_name, "object"))
    {
      return get_attribute_value (parent, "class");
    }
  else if (g_str_equal (parent->element_name, "template"))
    {
      return get_attribute_value (parent, "parent");
    }

  return NULL;
}

static gboolean
property_is_boolean (Element      *element,
                     MyParserData *data)
{
  GParamSpec *pspec;
  const char *class_name;
  const char *property_name;
  int i;
  PropKind kind;

  kind = get_prop_kind (element);
  class_name = get_class_name (element);
  property_name = "";

  for (i = 0; element->attribute_names[i]; i++)
    {
      if (strcmp (element->attribute_names[i], "name") == 0)
        property_name = (const gchar *)element->attribute_values[i];
    }

  pspec = get_property_pspec (data, class_name, property_name, kind);
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
  GParamSpec *pspec;
  PropKind kind;

  kind = get_prop_kind (element);
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

  if (data->convert3to4 &&
      keep_for_rewrite (class_name, property_name, kind))
    return FALSE; /* keep, will be rewritten */

  if (translatable)
    return FALSE;

  if (bound)
    return FALSE;

  pspec = get_property_pspec (data, class_name, property_name, kind);

  if (pspec == NULL)
    {
      const char *kind_str[] = {
        "",
        "Packing ",
        "Cell ",
        "Layout "
      };

      g_printerr (_("%s: %sproperty %s::%s not found\n"),
                  data->input_filename, kind_str[kind], class_name, property_name);
      return FALSE;
    }

  if (needs_explicit_setting (pspec, kind))
    return FALSE;

  return value_is_default (element, data, pspec, value_string);
}

static gboolean
property_has_been_removed (Element      *element,
                           MyParserData *data)
{
  const gchar *class_name;
  const gchar *property_name;
  struct _Prop {
    const char *class;
    const char *property;
    PropKind kind;
  } props[] = {
    { "GtkActionBar", "position", PROP_KIND_PACKING },
    { "GtkButtonBox", "secondary", PROP_KIND_PACKING },
    { "GtkButtonBox", "non-homogeneous", PROP_KIND_PACKING },
    { "GtkBox", "position", PROP_KIND_PACKING },
    { "GtkBox", "pack-type", PROP_KIND_PACKING },
    { "GtkHeaderBar", "position", PROP_KIND_PACKING },
    { "GtkPopoverMenu", "position",PROP_KIND_PACKING },
    { "GtkMenu", "left-attach", PROP_KIND_PACKING },
    { "GtkMenu", "right-attach", PROP_KIND_PACKING },
    { "GtkMenu", "top-attach", PROP_KIND_PACKING },
    { "GtkMenu", "bottom-attach", PROP_KIND_PACKING }
  };
  gchar *canonical_name;
  gboolean found;
  gint i, k;
  PropKind kind;

  kind = get_prop_kind (element);

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
          kind == props[k].kind)
        {
          found = TRUE;
          break;
        }
    }

  g_free (canonical_name);

  return found;
}

static void
maybe_rename_property (Element *element, MyParserData *data)
{
  const gchar *class_name;
  const gchar *property_name;
  struct _Prop {
    const char *class;
    const char *property;
    PropKind kind;
    const char *new_name;
  } props[] = {
    { "GtkPopover", "modal", PROP_KIND_OBJECT, "autohide" },
  };
  char *canonical_name;
  int i, k;
  PropKind kind;
  int prop_name_index = 0;

  kind = get_prop_kind (element);

  class_name = get_class_name (element);
  property_name = NULL;

  for (i = 0; element->attribute_names[i]; i++)
    {
      if (strcmp (element->attribute_names[i], "name") == 0)
        {
          prop_name_index = i;
          property_name = (const gchar *)element->attribute_values[i];
        }
    }

  if (property_name == NULL)
    return;

  canonical_name = g_strdup (property_name);
  g_strdelimit (canonical_name, "_", '-');

  for (k = 0; k < G_N_ELEMENTS (props); k++)
    {
      if (strcmp (class_name, props[k].class) == 0 &&
          strcmp (canonical_name, props[k].property) == 0 &&
          kind == props[k].kind)
        {
          g_free (element->attribute_values[prop_name_index]);
          element->attribute_values[prop_name_index] = g_strdup (props[k].new_name);
          break;
        }
    }

  g_free (canonical_name);
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
  new_object->parent = child;
  packing->children = NULL;

  prop = g_new0 (Element, 1);
  prop->element_name = g_strdup ("property");
  prop->attribute_names = g_new0 (char *, 2);
  prop->attribute_names[0] = g_strdup ("name");
  prop->attribute_values = g_new0 (char *, 2);
  prop->attribute_values[0] = g_strdup ("child");
  prop->children = g_list_append (prop->children, object);
  prop->parent = new_object;
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
  new_object->parent = child;
  packing->children = NULL;

  prop = g_new0 (Element, 1);
  prop->element_name = g_strdup ("property");
  prop->attribute_names = g_new0 (char *, 2);
  prop->attribute_names[0] = g_strdup ("name");
  prop->attribute_values = g_new0 (char *, 2);
  prop->attribute_values[0] = g_strdup ("child");
  prop->children = g_list_append (prop->children, object);
  prop->parent = new_object;
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
  new_object->parent = child;
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
  prop->parent = new_object;
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
      prop->parent = new_object;
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

static void
rewrite_pack_type_child (Element *element,
                         MyParserData *data)
{
  Element *pack_type = NULL;
  GList *l, *ll;

  if (!g_str_equal (element->element_name, "child"))
    return;

  for (l = element->children; l; l = l->next)
    {
      Element *elt = l->data;

      if (g_str_equal (elt->element_name, "packing"))
        {
          for (ll = elt->children; ll; ll = ll->next)
            {
              Element *elt2 = ll->data;

              if (g_str_equal (elt2->element_name, "property") &&
                  has_attribute (elt2, "name", "pack-type"))
                {
                  pack_type = elt2;
                  elt->children = g_list_remove (elt->children, pack_type);
                  if (elt->children == NULL)
                    {
                      element->children = g_list_remove (element->children, elt);
                      free_element (elt);
                    }
                  break;
                }
            }
        }

      if (pack_type)
        break;
    }

  if (pack_type)
    {
      char **attnames = g_new0 (char *, g_strv_length (element->attribute_names) + 2);
      char **attvalues = g_new0 (char *, g_strv_length (element->attribute_names) + 2);
      int i;

      for (i = 0; element->attribute_names[i]; i++)
        {
          attnames[i] = g_strdup (element->attribute_names[i]);
          attvalues[i] = g_strdup (element->attribute_values[i]);
        }

      attnames[i] = g_strdup ("type");
      attvalues[i] = g_strdup (pack_type->data);

      g_strfreev (element->attribute_names);
      g_strfreev (element->attribute_values);

      element->attribute_names = attnames;
      element->attribute_values = attvalues;

      free_element (pack_type);
    }
}

static void
rewrite_pack_type (Element *element,
                   MyParserData *data)
{
  GList *l;

  for (l = element->children; l; l = l->next)
    {
      Element *elt = l->data;
      if (g_str_equal (elt->element_name, "child"))
        rewrite_pack_type_child (elt, data); 
    }
}

static void
rewrite_child_prop_to_prop_child (Element *element,
                                  MyParserData *data,
                                  const char *child_prop,
                                  const char *prop)
{
  Element *object = NULL;
  Element *replaced = NULL;
  GList *l, *ll;

  if (!g_str_equal (element->element_name, "child"))
    return;

  for (l = element->children; l; l = l->next)
    {
      Element *elt = l->data;

      if (g_str_equal (elt->element_name, "object"))
        object = elt;

      if (g_str_equal (elt->element_name, "packing"))
        {
          for (ll = elt->children; ll; ll = ll->next)
            {
              Element *elt2 = ll->data;

              if (g_str_equal (elt2->element_name, "property") &&
                  has_attribute (elt2, "name", child_prop))
                {
                  replaced = elt2;
                  elt->children = g_list_remove (elt->children, replaced);
                  if (elt->children == NULL)
                    {
                      element->children = g_list_remove (element->children, elt);
                      free_element (elt);
                    }
                  break;
                }
            }
        }

      if (replaced)
        break;
    }

  if (replaced)
    {
      Element *elt;

      elt = g_new0 (Element, 1);
      elt->parent = element;
      elt->element_name = g_strdup ("property");
      elt->attribute_names = g_new0 (char *, 2);
      elt->attribute_names[0] = g_strdup ("name");
      elt->attribute_values = g_new0 (char *, 2);
      elt->attribute_values[0] = g_strdup (prop);
      elt->data = g_strdup (replaced->data);

      object->children = g_list_prepend (object->children, elt);

      free_element (replaced);
    }
}

static void
rewrite_child_prop_to_prop (Element *element,
                            MyParserData *data,
                            const char *child_prop,
                            const char *prop)
{
  GList *l;

  for (l = element->children; l; l = l->next)
    {
      Element *elt = l->data;
      if (g_str_equal (elt->element_name, "child"))
        rewrite_child_prop_to_prop_child (elt, data, child_prop, prop); 
    }
}

static void
rewrite_paned_child (Element *element,
                     MyParserData *data,
                     Element *child,
                     const char *suffix)
{
  Element *resize = NULL;
  Element *shrink = NULL;
  GList *l, *ll;

  for (l = child->children; l; l = l->next)
    {
      Element *elt = l->data;

      if (g_str_equal (elt->element_name, "packing"))
        {
          for (ll = elt->children; ll; ll = ll->next)
            {
              Element *elt2 = ll->data;

              if (g_str_equal (elt2->element_name, "property") &&
                  has_attribute (elt2, "name", "resize"))
                resize = elt2;
              if (g_str_equal (elt2->element_name, "property") &&
                  has_attribute (elt2, "name", "shrink"))
                shrink = elt2;
            }
          if (resize)
            elt->children = g_list_remove (elt->children, resize);
          if (shrink)
            elt->children = g_list_remove (elt->children, shrink);
          if (elt->children == NULL)
            {
              child->children = g_list_remove (child->children, elt);
              free_element (elt);
            }
        }

      if (resize || shrink)
        break;
    }

  if (resize)
    {
      Element *elt;

      elt = g_new0 (Element, 1);
      elt->parent = element;
      elt->element_name = g_strdup ("property");
      elt->attribute_names = g_new0 (char *, 2);
      elt->attribute_names[0] = g_strdup ("name");
      elt->attribute_values = g_new0 (char *, 2);
      elt->attribute_values[0] = g_strconcat ("resize-", suffix, NULL);
      elt->data = g_strdup (resize->data);

      element->children = g_list_prepend (element->children, elt);

      free_element (resize);
    }

  if (shrink)
    {
      Element *elt;

      elt = g_new0 (Element, 1);
      elt->parent = element;
      elt->element_name = g_strdup ("property");
      elt->attribute_names = g_new0 (char *, 2);
      elt->attribute_names[0] = g_strdup ("name");
      elt->attribute_values = g_new0 (char *, 2);
      elt->attribute_values[0] = g_strconcat ("shrink-", suffix, NULL);
      elt->data = g_strdup (shrink->data);

      element->children = g_list_prepend (element->children, elt);

      free_element (shrink);
    }
}

static void
rewrite_paned (Element *element,
               MyParserData *data)
{
  Element *child1 = NULL;
  Element *child2 = NULL;
  GList *l;

  for (l = element->children; l; l = l->next)
    {
      Element *elt = l->data;

      if (g_str_equal (elt->element_name, "child"))
        {
          if (child1 == NULL)
            child1 = elt;
          else if (child2 == NULL)
            child2 = elt;
          else
            break;
        }
    }

  if (child1)
    rewrite_paned_child (element, data, child1, "child1");

  if (child2)
    rewrite_paned_child (element, data, child2, "child2");
}

static void
rewrite_dialog (Element *element,
               MyParserData *data)
{
  Element *content_area = NULL;
  Element *vbox = NULL;
  Element *action_area = NULL;
  GList *l;

  for (l = element->children; l; l = l->next)
    {
      Element *elt = l->data;

      if (g_str_equal (elt->element_name, "child") &&
          g_strcmp0 (get_attribute_value (elt, "internal-child"), "vbox") == 0)
        {
          content_area = elt;
          break;
        }
    }

  if (!content_area || !content_area->children)
    return;

  vbox = content_area->children->data;

  for (l = vbox->children; l; l = l->next)
    {
      Element *elt = l->data;

      if (g_str_equal (elt->element_name, "child") &&
          g_strcmp0 (get_attribute_value (elt, "internal-child"), "action_area") == 0)
        {
          action_area = elt;
          break;
        }
    }

  if (!action_area)
    return;

  set_attribute_value (content_area, "internal-child", "content_area");
  vbox->children = g_list_remove (vbox->children, action_area);
  action_area->parent = element;
  element->children = g_list_append (element->children, action_area);

  for (l = action_area->children; l; l = l->next)
    {
      Element *elt = l->data;

      if (g_str_equal (elt->element_name, "packing"))
        {
          action_area->children = g_list_remove (action_area->children, elt);
          free_element (elt);
          break;
        }
    }

}

static void
rewrite_layout_props (Element *element,
                      MyParserData *data)
{
  GList *l, *ll;

  for (l = element->children; l; l = l->next)
    {
      Element *child = l->data;

      if (g_str_equal (child->element_name, "child"))
        {
          Element *object = NULL;
          Element *packing = NULL;

          for (ll = child->children; ll; ll = ll->next)
            {
              Element *elt2 = ll->data;

              if (g_str_equal (elt2->element_name, "object"))
                object = elt2;

              if (g_str_equal (elt2->element_name, "packing"))
                packing = elt2;
            }

          if (object && packing)
            {
              child->children = g_list_remove (child->children, packing);

              g_free (packing->element_name);
              packing->element_name = g_strdup ("layout");

              packing->parent = object;
              object->children = g_list_append (object->children, packing);
            }
        }
    }
}

static void
rewrite_grid_layout_prop (Element *element,
                          const char *attr_name,
                          const char *old_value,
                          const char *new_value)
{
  char *canonical_name;

  canonical_name = g_strdup (old_value);
  g_strdelimit (canonical_name, "_", '-');

  if (g_str_equal (element->element_name, "property"))
    {
      if (has_attribute (element, attr_name, old_value) ||
          has_attribute (element, attr_name, canonical_name))
        {
          set_attribute_value (element, attr_name, new_value);
        }
    }

  g_free (canonical_name);
}

static void
rewrite_grid_layout (Element *element,
                     MyParserData *data)
{
  struct _Prop {
    const char *attr_name;
    const char *old_value;
    const char *new_value;
  } props[] = {
    { "name", "width", "column-span", },
    { "name", "height", "row-span", },
  };
  GList *l, *ll;

  for (l = element->children; l; l = l->next)
    {
      Element *child = l->data;

      if (g_str_equal (child->element_name, "child"))
        {
          Element *object = NULL;
          Element *packing = NULL;

          for (ll = child->children; ll; ll = ll->next)
            {
              Element *elt2 = ll->data;

              if (g_str_equal (elt2->element_name, "object"))
                object = elt2;

              if (g_str_equal (elt2->element_name, "packing"))
                packing = elt2;
            }

          if (object && packing)
            {
              int i;

              child->children = g_list_remove (child->children, packing);

              g_free (packing->element_name);
              packing->element_name = g_strdup ("layout");

              packing->parent = object;
              object->children = g_list_append (object->children, packing);

              for (ll = packing->children; ll; ll = ll->next)
                {
                  Element *elt = ll->data;

                  for (i = 0; i < G_N_ELEMENTS (props); i++)
                    rewrite_grid_layout_prop (elt,
                                              props[i].attr_name,
                                              props[i].old_value,
                                              props[i].new_value);
                }
            }
        }
    }
}

/* returns TRUE to remove the element from the parent */
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

  return FALSE;
}

static void
simplify_tree (MyParserData *data)
{
  simplify_element (data->root, data);
}

static gboolean
rewrite_element (Element      *element,
                 MyParserData *data)
{
  GList *l;

  l = element->children;
  while (l)
    {
      GList *next = l->next;
      Element *child = l->data;
      if (rewrite_element (child, data))
        {
          element->children = g_list_remove (element->children, child);
          free_element (child);
        }
      l = next;
    }

  if (element_is_object_or_template (element) &&
      g_str_equal (get_class_name (element), "GtkStack"))
    rewrite_stack (element, data);

  if (element_is_object_or_template (element) &&
      g_str_equal (get_class_name (element), "GtkAssistant"))
    rewrite_assistant (element, data);

  if (element_is_object_or_template (element) &&
      g_str_equal (get_class_name (element), "GtkNotebook"))
    rewrite_notebook (element, data);

  if (element_is_object_or_template (element) &&
      (g_str_equal (get_class_name (element), "GtkActionBar") ||
       g_str_equal (get_class_name (element), "GtkHeaderBar")))
    rewrite_pack_type (element, data);

  if (element_is_object_or_template (element) &&
      g_str_equal (get_class_name (element), "GtkPopoverMenu"))
    rewrite_child_prop_to_prop (element, data, "submenu", "name");

  if (element_is_object_or_template (element) &&
      g_str_equal (get_class_name (element), "GtkToolbar"))
    rewrite_child_prop_to_prop (element, data, "expand", "expand-item");

  if (element_is_object_or_template (element) &&
      g_str_equal (get_class_name (element), "GtkToolbar"))
    rewrite_child_prop_to_prop (element, data, "homogeneous", "homogeneous");

  if (element_is_object_or_template (element) &&
      g_str_equal (get_class_name (element), "GtkPaned"))
    rewrite_paned (element, data);

  if (element_is_object_or_template (element) &&
      g_str_equal (get_class_name (element), "GtkDialog"))
    rewrite_dialog (element, data);

  if (element_is_object_or_template (element) &&
      g_str_equal (get_class_name (element), "GtkOverlay"))
    rewrite_layout_props (element, data);

  if (element_is_object_or_template (element) &&
      g_str_equal (get_class_name (element), "GtkGrid"))
    rewrite_grid_layout (element, data);

  if (element_is_object_or_template (element) &&
      g_str_equal (get_class_name (element), "GtkFixed"))
    rewrite_layout_props (element, data);

  if (g_str_equal (element->element_name, "property"))
    maybe_rename_property (element, data);

  if (g_str_equal (element->element_name, "property") &&
      property_has_been_removed (element, data))
    return TRUE;

  return FALSE;
}

static void
rewrite_tree (MyParserData *data)
{
  rewrite_element (data->root, data);
}

/* For properties which have changed their default
 * value between 3 and 4, we make sure that their
 * old default value is present in the tree before
 * simplifying it.
 *
 * So far, this is just GtkWidget::visible,
 * changing its default from 0 to 1.
 */
static void
add_old_default_properties (Element      *element,
                            MyParserData *data)
{
  const char *class_name;
  GType type;

  if (!g_str_equal (element->element_name, "object"))
    return;

  class_name = get_class_name (element);
  type = g_type_from_name (class_name);
  if (g_type_is_a (type, GTK_TYPE_WIDGET))
    {
      GList *l;
      gboolean has_visible = FALSE;

      for (l = element->children; l; l = l->next)
        {
          Element *prop = l->data;
          const char *name = get_attribute_value (prop, "name");

          if (g_str_equal (prop->element_name, "property") &&
              g_str_equal (name, "visible"))
            has_visible = TRUE;
        }

      if (!has_visible)
        {
          Element *new_prop = g_new0 (Element, 1);
          new_prop->parent = element;
          new_prop->element_name = g_strdup ("property");
          new_prop->attribute_names = g_new0 (char *, 2);
          new_prop->attribute_names[0] = g_strdup ("name");
          new_prop->attribute_values = g_new0 (char *, 2);
          new_prop->attribute_values[0] = g_strdup ("visible");
          new_prop->data = g_strdup ("0");
          element->children = g_list_prepend (element->children, new_prop);
        }
    }
}

static void
enhance_element (Element      *element,
                 MyParserData *data)
{
  GList *l;

  add_old_default_properties (element, data);

  for (l = element->children; l; l = l->next)
    {
      Element *child = l->data;
      enhance_element (child, data);
    }
}

static void
enhance_tree (MyParserData *data)
{
  enhance_element (data->root, data);
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
write_xml_declaration (FILE *output)
{
  g_fprintf (output, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
}

static void
dump_tree (MyParserData *data)
{
  write_xml_declaration (data->output);
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

  if (data.convert3to4)
    {
      enhance_tree (&data);
      rewrite_tree (&data);
    }
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
