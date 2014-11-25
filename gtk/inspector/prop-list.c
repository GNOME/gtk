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
  COLUMN_DEFINED_AT,
  COLUMN_TOOLTIP,
  COLUMN_WRITABLE,
  COLUMN_ATTRIBUTE
};

enum
{
  PROP_0,
  PROP_OBJECT_TREE,
  PROP_CHILD_PROPERTIES
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
  GtkWidget *object_title;
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
      gtk_stack_set_visible_child_name (GTK_STACK (pl->priv->search_stack), "search");
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
  gtk_tree_view_set_search_entry (GTK_TREE_VIEW (pl->priv->tree),
                                  GTK_ENTRY (pl->priv->search_entry));
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

  gtk_widget_show (popover);

  g_signal_connect (popover, "hide", G_CALLBACK (gtk_widget_destroy), NULL);

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
gtk_inspector_prop_list_class_init (GtkInspectorPropListClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = finalize;
  object_class->get_property = get_property;
  object_class->set_property = set_property;

  g_object_class_install_property (object_class, PROP_OBJECT_TREE,
      g_param_spec_object ("object-tree", "Object Tree", "Object tree",
                           GTK_TYPE_WIDGET, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_CHILD_PROPERTIES,
      g_param_spec_boolean ("child-properties", "Child properties", "Child properties",
                            FALSE, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/inspector/prop-list.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorPropList, model);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorPropList, attribute_column);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorPropList, tree);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorPropList, search_entry);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorPropList, search_stack);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorPropList, object_title);
  gtk_widget_class_bind_template_callback (widget_class, row_activated);
  gtk_widget_class_bind_template_callback (widget_class, search_close_clicked);
  gtk_widget_class_bind_template_callback (widget_class, hierarchy_changed);
}

static void
gtk_inspector_prop_list_update_prop (GtkInspectorPropList *pl,
                                     GtkTreeIter          *iter,
                                     GParamSpec           *prop)
{
  GValue gvalue = {0};
  gchar *value = NULL;
  gchar *attribute = NULL;

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

  if (G_VALUE_HOLDS_ENUM (&gvalue))
    {
      GEnumClass *enum_class = G_PARAM_SPEC_ENUM (prop)->enum_class;
      GEnumValue *enum_value = g_enum_get_value (enum_class, g_value_get_enum (&gvalue));

      value = g_strdup (enum_value->value_name);
    }
  else
    {
      value = g_strdup_value_contents (&gvalue);
    }

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

  gtk_list_store_set (pl->priv->model, iter,
                      COLUMN_NAME, prop->name,
                      COLUMN_VALUE, value ? value : "",
                      COLUMN_DEFINED_AT, g_type_name (prop->owner_type),
                      COLUMN_TOOLTIP, g_param_spec_get_blurb (prop),
                      COLUMN_WRITABLE, (prop->flags & G_PARAM_WRITABLE) != 0,
                      COLUMN_ATTRIBUTE, attribute ? attribute : "",
                      -1);

  g_free (value);
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
  const gchar *title;

  if (!object)
    return FALSE;

  if (pl->priv->object == object)
    return TRUE;

  cleanup_object (pl);

  if (!object)
    {
      gtk_widget_hide (GTK_WIDGET (pl));
      return TRUE;
    }

  title = (const gchar *)g_object_get_data (object, "gtk-inspector-object-title");
  gtk_label_set_label (GTK_LABEL (pl->priv->object_title), title);

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
