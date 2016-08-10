/*
 * Copyright (c) 2008-2009  Christian Hammond
 * Copyright (c) 2008-2009  David Trowbridge
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "config.h"
#include <glib/gi18n-lib.h>

#include "prop-list.h"

#include "prop-editor.h"
#include "object-tree.h"

#include "gtkcelllayout.h"
#include "gtktreeview.h"
#include "gtktreeselection.h"
#include "gtkpopover.h"
#include "gtksearchentry.h"
#include "gtklabel.h"
#include "gtkstack.h"

enum
{
  COLUMN_NAME,
  COLUMN_VALUE,
  COLUMN_TYPE,
  COLUMN_DEFINED_AT,
  COLUMN_TOOLTIP,
  COLUMN_WRITABLE,
  COLUMN_ATTRIBUTE
};

enum
{
  PROP_0,
  PROP_OBJECT_TREE,
  PROP_CHILD_PROPERTIES,
  PROP_SEARCH_ENTRY
};

struct _GtkInspectorPropListPrivate
{
  GObject *object;
  GtkListStore *model;
  GHashTable *prop_iters;
  gulong notify_handler_id;
  GtkInspectorObjectTree *object_tree;
  gboolean child_properties;
  GtkTreeViewColumn *name_column;
  GtkTreeViewColumn *attribute_column;
  GtkWidget *tree;
  GtkWidget *search_entry;
  GtkWidget *search_stack;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorPropList, gtk_inspector_prop_list, GTK_TYPE_BOX)

static void
search_close_clicked (GtkWidget            *button,
                      GtkInspectorPropList *pl)
{
  gtk_entry_set_text (GTK_ENTRY (pl->priv->search_entry), "");
  gtk_stack_set_visible_child_name (GTK_STACK (pl->priv->search_stack), "title");
}

static gboolean
key_press_event (GtkWidget            *window,
                 GdkEvent             *event,
                 GtkInspectorPropList *pl)
{
  if (!gtk_widget_get_mapped (GTK_WIDGET (pl)))
    return GDK_EVENT_PROPAGATE;

  if (gtk_search_entry_handle_event (GTK_SEARCH_ENTRY (pl->priv->search_entry), event))
    {
      gtk_stack_set_visible_child (GTK_STACK (pl->priv->search_stack), pl->priv->search_entry);
      return GDK_EVENT_STOP;
    }
  return GDK_EVENT_PROPAGATE;
}

static void
hierarchy_changed (GtkWidget *widget,
                   GtkWidget *previous_toplevel)
{
  if (previous_toplevel)
    g_signal_handlers_disconnect_by_func (previous_toplevel, key_press_event, widget);
  g_signal_connect (gtk_widget_get_toplevel (widget), "key-press-event",
                    G_CALLBACK (key_press_event), widget);
}

static void
gtk_inspector_prop_list_init (GtkInspectorPropList *pl)
{
  pl->priv = gtk_inspector_prop_list_get_instance_private (pl);
  gtk_widget_init_template (GTK_WIDGET (pl));
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (pl->priv->model),
                                        COLUMN_NAME,
                                        GTK_SORT_ASCENDING);
  pl->priv->prop_iters = g_hash_table_new_full (g_str_hash,
                                                g_str_equal,
                                                NULL,
                                                (GDestroyNotify) gtk_tree_iter_free);
}

static void
get_property (GObject    *object,
              guint       param_id,
              GValue     *value,
              GParamSpec *pspec)
{
  GtkInspectorPropList *pl = GTK_INSPECTOR_PROP_LIST (object);

  switch (param_id)
    {
      case PROP_OBJECT_TREE:
        g_value_take_object (value, pl->priv->object_tree);
        break;

      case PROP_CHILD_PROPERTIES:
        g_value_set_boolean (value, pl->priv->child_properties);
        break;

      case PROP_SEARCH_ENTRY:
        g_value_take_object (value, pl->priv->search_entry);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
set_property (GObject      *object,
              guint         param_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  GtkInspectorPropList *pl = GTK_INSPECTOR_PROP_LIST (object);

  switch (param_id)
    {
      case PROP_OBJECT_TREE:
        pl->priv->object_tree = g_value_get_object (value);
        break;

      case PROP_CHILD_PROPERTIES:
        pl->priv->child_properties = g_value_get_boolean (value);
        break;

      case PROP_SEARCH_ENTRY:
        pl->priv->search_entry = g_value_get_object (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
show_object (GtkInspectorPropEditor *editor,
             GObject                *object,
             const gchar            *name,
             const gchar            *tab,
             GtkInspectorPropList   *pl)
{
  GtkTreeIter iter;
  GtkWidget *popover;

  popover = gtk_widget_get_ancestor (GTK_WIDGET (editor), GTK_TYPE_POPOVER);
  gtk_widget_hide (popover);

  g_object_set_data (G_OBJECT (pl->priv->object_tree), "next-tab", (gpointer)tab);
  if (gtk_inspector_object_tree_find_object (pl->priv->object_tree, object, &iter))
    {
      gtk_inspector_object_tree_select_object (pl->priv->object_tree, object);
    }
  else if (gtk_inspector_object_tree_find_object (pl->priv->object_tree, pl->priv->object, &iter))
    {
      gtk_inspector_object_tree_append_object (pl->priv->object_tree, object, &iter, name);
      gtk_inspector_object_tree_select_object (pl->priv->object_tree, object);
    }
  else
    {
      g_warning ("GtkInspector: couldn't find the widget in the tree");
    }
}

static void
row_activated (GtkTreeView *tv,
               GtkTreePath *path,
               GtkTreeViewColumn *col,
               GtkInspectorPropList *pl)
{
  GtkTreeIter iter;
  GdkRectangle rect;
  gchar *name;
  GtkWidget *editor;
  GtkWidget *popover;

  gtk_tree_model_get_iter (GTK_TREE_MODEL (pl->priv->model), &iter, path);
  gtk_tree_model_get (GTK_TREE_MODEL (pl->priv->model), &iter, COLUMN_NAME, &name, -1);
  gtk_tree_view_get_cell_area (tv, path, col, &rect);
  gtk_tree_view_convert_bin_window_to_widget_coords (tv, rect.x, rect.y, &rect.x, &rect.y);

  popover = gtk_popover_new (GTK_WIDGET (tv));
  gtk_popover_set_pointing_to (GTK_POPOVER (popover), &rect);

  editor = gtk_inspector_prop_editor_new (pl->priv->object, name, pl->priv->child_properties);
  gtk_widget_show (editor);

  gtk_container_add (GTK_CONTAINER (popover), editor);

  if (gtk_inspector_prop_editor_should_expand (GTK_INSPECTOR_PROP_EDITOR (editor)))
    gtk_widget_set_vexpand (popover, TRUE);

  g_signal_connect (editor, "show-object", G_CALLBACK (show_object), pl);

  gtk_popover_popup (GTK_POPOVER (popover));

  g_signal_connect (popover, "unmap", G_CALLBACK (gtk_widget_destroy), NULL);

  g_free (name);
}

static void cleanup_object (GtkInspectorPropList *pl);

static void
finalize (GObject *object)
{
  GtkInspectorPropList *pl = GTK_INSPECTOR_PROP_LIST (object);

  cleanup_object (pl);
  g_hash_table_unref (pl->priv->prop_iters);

  G_OBJECT_CLASS (gtk_inspector_prop_list_parent_class)->finalize (object);
}

static void
constructed (GObject *object)
{
  GtkInspectorPropList *pl = GTK_INSPECTOR_PROP_LIST (object);

  pl->priv->search_stack = gtk_widget_get_parent (pl->priv->search_entry);

  gtk_tree_view_set_search_entry (GTK_TREE_VIEW (pl->priv->tree),
                                  GTK_ENTRY (pl->priv->search_entry));

  g_signal_connect (pl->priv->search_entry, "stop-search",
                    G_CALLBACK (search_close_clicked), pl);
}

static void
gtk_inspector_prop_list_class_init (GtkInspectorPropListClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = finalize;
  object_class->get_property = get_property;
  object_class->set_property = set_property;
  object_class->constructed = constructed;

  g_object_class_install_property (object_class, PROP_OBJECT_TREE,
      g_param_spec_object ("object-tree", "Object Tree", "Object tree",
                           GTK_TYPE_WIDGET, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_CHILD_PROPERTIES,
      g_param_spec_boolean ("child-properties", "Child properties", "Child properties",
                            FALSE, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_SEARCH_ENTRY,
      g_param_spec_object ("search-entry", "Search Entry", "Search Entry",
                           GTK_TYPE_WIDGET, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/prop-list.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorPropList, model);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorPropList, attribute_column);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorPropList, tree);
  gtk_widget_class_bind_template_callback (widget_class, row_activated);
  gtk_widget_class_bind_template_callback (widget_class, search_close_clicked);
  gtk_widget_class_bind_template_callback (widget_class, hierarchy_changed);
}

/* Like g_strdup_value_contents, but keeps the type name separate */
static void
strdup_value_contents (const GValue  *value,
                       gchar        **contents,
                       gchar        **type)
{
  const gchar *src;

  if (G_VALUE_HOLDS_STRING (value))
    {
      src = g_value_get_string (value);

      *type = g_strdup ("char*");

      if (!src)
        {
          *contents = g_strdup ("NULL");
        }
      else
        {
          gchar *s = g_strescape (src, NULL);
          *contents = g_strdup_printf ("\"%s\"", s);
          g_free (s);
        }
    }
  else if (g_value_type_transformable (G_VALUE_TYPE (value), G_TYPE_STRING))
    {
      GValue tmp_value = G_VALUE_INIT;

      *type = g_strdup (g_type_name (G_VALUE_TYPE (value)));

      g_value_init (&tmp_value, G_TYPE_STRING);
      g_value_transform (value, &tmp_value);
      src = g_value_get_string (&tmp_value);
      if (!src)
        *contents = g_strdup ("NULL");
      else
        *contents = g_strescape (src, NULL);
      g_value_unset (&tmp_value);
    }
  else if (g_value_fits_pointer (value))
    {
      gpointer p = g_value_peek_pointer (value);

      if (!p)
        {
          *type = g_strdup (g_type_name (G_VALUE_TYPE (value)));
          *contents = g_strdup ("NULL");
        }
      else if (G_VALUE_HOLDS_OBJECT (value))
        {
          *type = g_strdup (G_OBJECT_TYPE_NAME (p));
          *contents = g_strdup_printf ("%p", p);
        }
      else if (G_VALUE_HOLDS_PARAM (value))
        {
          *type = g_strdup (G_PARAM_SPEC_TYPE_NAME (p));
          *contents = g_strdup_printf ("%p", p);
        }
      else if (G_VALUE_HOLDS (value, G_TYPE_STRV))
        {
          GStrv strv = g_value_get_boxed (value);
          GString *tmp = g_string_new ("[");

          while (*strv != NULL)
            {
              gchar *escaped = g_strescape (*strv, NULL);

              g_string_append_printf (tmp, "\"%s\"", escaped);
              g_free (escaped);

              if (*++strv != NULL)
                g_string_append (tmp, ", ");
            }

          g_string_append (tmp, "]");
          *type = g_strdup ("char**");
          *contents = g_string_free (tmp, FALSE);
        }
      else if (G_VALUE_HOLDS_BOXED (value))
        {
          *type = g_strdup (g_type_name (G_VALUE_TYPE (value)));
          *contents = g_strdup_printf ("%p", p);
        }
      else if (G_VALUE_HOLDS_POINTER (value))
        {
          *type = g_strdup ("gpointer");
          *contents = g_strdup_printf ("%p", p);
        }
      else
        {
          *type = g_strdup ("???");
          *contents = g_strdup ("???");
        }
    }
  else
    {
      *type = g_strdup ("???");
      *contents = g_strdup ("???");
    }
}

static void
gtk_inspector_prop_list_update_prop (GtkInspectorPropList *pl,
                                     GtkTreeIter          *iter,
                                     GParamSpec           *prop)
{
  GValue gvalue = {0};
  gchar *value;
  gchar *type;
  gchar *attribute = NULL;
  gboolean writable;

  g_value_init (&gvalue, prop->value_type);
  if (pl->priv->child_properties)
    {
      GtkWidget *parent;

      parent = gtk_widget_get_parent (GTK_WIDGET (pl->priv->object));
      gtk_container_child_get_property (GTK_CONTAINER (parent),
                                        GTK_WIDGET (pl->priv->object),
                                        prop->name, &gvalue);
    }
  else
    g_object_get_property (pl->priv->object, prop->name, &gvalue);

  strdup_value_contents (&gvalue, &value, &type);

  if (GTK_IS_CELL_RENDERER (pl->priv->object))
    {
      gpointer *layout;
      GtkCellArea *area;
      gint column = -1;

      area = NULL;
      layout = g_object_get_data (pl->priv->object, "gtk-inspector-cell-layout");
      if (layout)
        area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (layout));
      if (area)
        column = gtk_cell_area_attribute_get_column (area,
                                                     GTK_CELL_RENDERER (pl->priv->object),
                                                     prop->name);

       if (column != -1)
         attribute = g_strdup_printf ("%d", column);
    }

  writable = ((prop->flags & G_PARAM_WRITABLE) != 0) &&
             ((prop->flags & G_PARAM_CONSTRUCT_ONLY) == 0);

  gtk_list_store_set (pl->priv->model, iter,
                      COLUMN_NAME, prop->name,
                      COLUMN_VALUE, value ? value : "",
                      COLUMN_TYPE, type ? type : "",
                      COLUMN_DEFINED_AT, g_type_name (prop->owner_type),
                      COLUMN_TOOLTIP, g_param_spec_get_blurb (prop),
                      COLUMN_WRITABLE, writable,
                      COLUMN_ATTRIBUTE, attribute ? attribute : "",
                      -1);

  g_free (value);
  g_free (type);
  g_free (attribute);
  g_value_unset (&gvalue);
}

static void
gtk_inspector_prop_list_prop_changed_cb (GObject              *pspec,
                                         GParamSpec           *prop,
                                         GtkInspectorPropList *pl)
{
  GtkTreeIter *iter;

  if (!pl->priv->object)
    return;

  iter = g_hash_table_lookup (pl->priv->prop_iters, prop->name);
  if (iter != NULL)
    gtk_inspector_prop_list_update_prop (pl, iter, prop);
}

static void
cleanup_object (GtkInspectorPropList *pl)
{
  if (pl->priv->object &&
      g_signal_handler_is_connected (pl->priv->object, pl->priv->notify_handler_id))
    g_signal_handler_disconnect (pl->priv->object, pl->priv->notify_handler_id);

  pl->priv->object = NULL;
  pl->priv->notify_handler_id = 0;

  g_hash_table_remove_all (pl->priv->prop_iters);
  if (pl->priv->model)
    gtk_list_store_clear (pl->priv->model);
}

gboolean
gtk_inspector_prop_list_set_object (GtkInspectorPropList *pl,
                                    GObject              *object)
{
  GtkTreeIter iter;
  GParamSpec **props;
  guint num_properties;
  guint i;

  if (!object)
    return FALSE;

  if (pl->priv->object == object)
    return TRUE;

  cleanup_object (pl);

  gtk_entry_set_text (GTK_ENTRY (pl->priv->search_entry), "");
  gtk_stack_set_visible_child_name (GTK_STACK (pl->priv->search_stack), "title");

  if (pl->priv->child_properties)
    {
      GtkWidget *parent;

      if (!GTK_IS_WIDGET (object))
        {
          gtk_widget_hide (GTK_WIDGET (pl));
          return TRUE;
        }

      parent = gtk_widget_get_parent (GTK_WIDGET (object));
      if (!parent)
        {
          gtk_widget_hide (GTK_WIDGET (pl));
          return TRUE;
        }

      gtk_tree_view_column_set_visible (pl->priv->attribute_column, FALSE);

      props = gtk_container_class_list_child_properties (G_OBJECT_GET_CLASS (parent), &num_properties);
    }
  else
    {
      gtk_tree_view_column_set_visible (pl->priv->attribute_column, GTK_IS_CELL_RENDERER (object));

      props = g_object_class_list_properties (G_OBJECT_GET_CLASS (object), &num_properties);
    }

  pl->priv->object = object;

  for (i = 0; i < num_properties; i++)
    {
      GParamSpec *prop = props[i];

      if (! (prop->flags & G_PARAM_READABLE))
        continue;

      gtk_list_store_append (pl->priv->model, &iter);
      gtk_inspector_prop_list_update_prop (pl, &iter, prop);

      g_hash_table_insert (pl->priv->prop_iters, (gpointer) prop->name, gtk_tree_iter_copy (&iter));
    }

  g_free (props);

  if (GTK_IS_WIDGET (object))
    g_signal_connect_object (object, "destroy", G_CALLBACK (cleanup_object), pl, G_CONNECT_SWAPPED);

  /* Listen for updates */
  pl->priv->notify_handler_id =
      g_signal_connect_object (object,
                               pl->priv->child_properties ? "child-notify" : "notify",
                               G_CALLBACK (gtk_inspector_prop_list_prop_changed_cb),
                               pl, 0);

  gtk_widget_show (GTK_WIDGET (pl));

  return TRUE;
}

// vim: set et sw=2 ts=2:
