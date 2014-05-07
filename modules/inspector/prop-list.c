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

#include "prop-list.h"
#include "property-cell-renderer.h"


enum
{
  COLUMN_NAME,
  COLUMN_VALUE,
  COLUMN_DEFINED_AT,
  COLUMN_OBJECT,
  COLUMN_TOOLTIP,
  COLUMN_WRITABLE
};

enum
{
  PROP_0,
  PROP_WIDGET_TREE,
  PROP_CHILD_PROPERTIES
};

struct _GtkInspectorPropListPrivate
{
  GObject *object;
  GtkListStore *model;
  GHashTable *prop_iters;
  GList *signal_cnxs;
  GtkWidget *widget_tree;
  GtkCellRenderer *value_renderer;
  gboolean child_properties;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorPropList, gtk_inspector_prop_list, GTK_TYPE_TREE_VIEW)

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
      case PROP_WIDGET_TREE:
        g_value_take_object (value, pl->priv->widget_tree);
        g_object_set_data (G_OBJECT (pl->priv->value_renderer), "gtk_inspector-widget-tree", pl->priv->widget_tree);
        break;

      case PROP_CHILD_PROPERTIES:
        g_value_set_boolean (value, pl->priv->child_properties);
        g_object_set (pl->priv->value_renderer,
                      "is-child-property", pl->priv->child_properties,
                      NULL);
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
      case PROP_WIDGET_TREE:
        pl->priv->widget_tree = g_value_get_object (value);
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
gtk_inspector_prop_list_class_init (GtkInspectorPropListClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = get_property;
  object_class->set_property = set_property;

  g_object_class_install_property (object_class, PROP_WIDGET_TREE,
      g_param_spec_object ("widget-tree", "Widget Tree", "Widget tree",
                           GTK_TYPE_WIDGET, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class, PROP_CHILD_PROPERTIES,
      g_param_spec_boolean ("child-properties", "Child properties", "Child properties",
                            FALSE, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/inspector/prop-list.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorPropList, model);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorPropList, value_renderer);
}

static void
gtk_inspector_prop_list_update_prop (GtkInspectorPropList *pl,
                                     GtkTreeIter          *iter,
                                     GParamSpec           *prop)
{
  GValue gvalue = {0};
  gchar *value;

  g_value_init(&gvalue, prop->value_type);
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
      value = g_strdup_value_contents(&gvalue);
    }

  gtk_list_store_set (pl->priv->model, iter,
                      COLUMN_NAME, prop->name,
                      COLUMN_VALUE, value ? value : g_strdup (""),
                      COLUMN_DEFINED_AT, g_type_name (prop->owner_type),
                      COLUMN_OBJECT, pl->priv->object,
                      COLUMN_TOOLTIP, g_param_spec_get_blurb (prop),
                      COLUMN_WRITABLE, (prop->flags & G_PARAM_WRITABLE) != 0,
                      -1);

  g_free (value);
  g_value_unset (&gvalue);
}

static void
gtk_inspector_prop_list_prop_changed_cb (GObject              *pspec,
                                         GParamSpec           *prop,
                                         GtkInspectorPropList *pl)
{
  GtkTreeIter *iter = g_hash_table_lookup(pl->priv->prop_iters, prop->name);

  if (iter != NULL)
    gtk_inspector_prop_list_update_prop (pl, iter, prop);
}

GtkWidget *
gtk_inspector_prop_list_new (GtkWidget *widget_tree,
                             gboolean   child_properties)
{
  g_type_ensure (GTK_TYPE_INSPECTOR_PROPERTY_CELL_RENDERER);

  return g_object_new (GTK_TYPE_INSPECTOR_PROP_LIST,
                       "widget-tree", widget_tree,
                       "child-properties", child_properties,
                       NULL);
}

gboolean
gtk_inspector_prop_list_set_object (GtkInspectorPropList *pl,
                                    GObject              *object)
{
  GtkTreeIter iter;
  GParamSpec **props;
  guint num_properties;
  guint i;
  GList *l;

  if (pl->priv->object == object)
    return FALSE;

  pl->priv->object = object;

  for (l = pl->priv->signal_cnxs; l != NULL; l = l->next)
    {
      gulong id = GPOINTER_TO_UINT (l->data);

      if (g_signal_handler_is_connected (object, id))
        g_signal_handler_disconnect (object, id);
    }

  g_list_free (pl->priv->signal_cnxs);
  pl->priv->signal_cnxs = NULL;

  g_hash_table_remove_all (pl->priv->prop_iters);
  gtk_list_store_clear (pl->priv->model);
  gtk_widget_set_sensitive (GTK_WIDGET (pl), FALSE);

  if (pl->priv->child_properties)
    {
      GtkWidget *parent;

      if (!GTK_IS_WIDGET (object))
        return TRUE;

      parent = gtk_widget_get_parent (GTK_WIDGET (object));
      if (!parent)
        return TRUE;

      props = gtk_container_class_list_child_properties (G_OBJECT_GET_CLASS (parent), &num_properties);
    }
  else
    props = g_object_class_list_properties (G_OBJECT_GET_CLASS (object), &num_properties);

  gtk_widget_set_sensitive (GTK_WIDGET (pl), TRUE);

  for (i = 0; i < num_properties; i++)
    {
      GParamSpec *prop = props[i];
      gchar *signal_name;

      if (! (prop->flags & G_PARAM_READABLE))
        continue;

      gtk_list_store_append (pl->priv->model, &iter);
      gtk_inspector_prop_list_update_prop (pl, &iter, prop);

      g_hash_table_insert (pl->priv->prop_iters, (gpointer) prop->name, gtk_tree_iter_copy (&iter));

      /* Listen for updates */
      if (pl->priv->child_properties)
        signal_name = g_strdup_printf ("child-notify::%s", prop->name);
      else
        signal_name = g_strdup_printf ("notify::%s", prop->name);

      pl->priv->signal_cnxs =
          g_list_prepend (pl->priv->signal_cnxs,
                          GINT_TO_POINTER (g_signal_connect(object, signal_name,
                                                            G_CALLBACK (gtk_inspector_prop_list_prop_changed_cb), pl)));

        g_free (signal_name);
    }

  return TRUE;
}

// vim: set et sw=2 ts=2:
