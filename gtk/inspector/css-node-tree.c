/*
 * Copyright (c) 2014 Benjamin Otte <otte@gnome.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicntnse,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright noticnt and this permission noticnt shall be included
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

#include "css-node-tree.h"
#include "prop-editor.h"

#include "gtktreemodelcssnode.h"
#include "gtk/gtktreeview.h"
#include "gtk/gtklabel.h"
#include "gtk/gtkpopover.h"
#include "gtk/gtkwidgetprivate.h"

enum {
  COLUMN_NAME,
  COLUMN_TYPE,
  COLUMN_VISIBLE,
  COLUMN_CLASSES,
  COLUMN_ID,
  /* add more */
  N_COLUMNS
};

struct _GtkInspectorCssNodeTreePrivate
{
  GtkWidget *tree_view;
  GtkTreeModel *model;
  GtkTreeViewColumn *name_column;
  GtkTreeViewColumn *id_column;
  GtkTreeViewColumn *classes_column;
  GtkWidget *object_title;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorCssNodeTree, gtk_inspector_css_node_tree, GTK_TYPE_BOX)

static void
row_activated (GtkTreeView             *tv,
               GtkTreePath             *path,
               GtkTreeViewColumn       *col,
               GtkInspectorCssNodeTree *cnt)
{
  GtkTreeIter iter;
  GdkRectangle rect;
  GtkWidget *editor;
  GtkWidget *popover;
  GtkCssNode *node;
  const gchar *prop_name;

  if (col == cnt->priv->name_column)
    prop_name = "name";
  else if (col == cnt->priv->id_column)
    prop_name = "id";
  else if (col == cnt->priv->classes_column)
    prop_name = "classes";
  else
    return;

  gtk_tree_model_get_iter (cnt->priv->model, &iter, path);
  node = gtk_tree_model_css_node_get_node_from_iter (GTK_TREE_MODEL_CSS_NODE (cnt->priv->model), &iter);
  gtk_tree_view_get_cell_area (tv, path, col, &rect);
  gtk_tree_view_convert_bin_window_to_widget_coords (tv, rect.x, rect.y, &rect.x, &rect.y);

  popover = gtk_popover_new (GTK_WIDGET (tv));
  gtk_popover_set_pointing_to (GTK_POPOVER (popover), &rect);

  editor = gtk_inspector_prop_editor_new (G_OBJECT (node), prop_name, FALSE);
  gtk_widget_show (editor);

  gtk_container_add (GTK_CONTAINER (popover), editor);

  if (gtk_inspector_prop_editor_should_expand (GTK_INSPECTOR_PROP_EDITOR (editor)))
    gtk_widget_set_vexpand (popover, TRUE);

  gtk_widget_show (popover);

  g_signal_connect (popover, "unmap", G_CALLBACK (gtk_widget_destroy), NULL);
}

static void
gtk_inspector_css_node_tree_finalize (GObject *object)
{
  //GtkInspectorCssNodeTree *cnt = GTK_INSPECTOR_CSS_NODE_TREE (object);

  G_OBJECT_CLASS (gtk_inspector_css_node_tree_parent_class)->finalize (object);
}

static void
gtk_inspector_css_node_tree_class_init (GtkInspectorCssNodeTreeClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gtk_inspector_css_node_tree_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/css-node-tree.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorCssNodeTree, tree_view);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorCssNodeTree, object_title);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorCssNodeTree, name_column);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorCssNodeTree, id_column);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorCssNodeTree, classes_column);

  gtk_widget_class_bind_template_callback (widget_class, row_activated);
}

static int
sort_strv (gconstpointer a,
           gconstpointer b,
           gpointer      data)
{
  char **ap = (char **) a;
  char **bp = (char **) b;

  return g_ascii_strcasecmp (*ap, *bp);
}

static void
strv_sort (char **strv)
{
  g_qsort_with_data (strv,
		     g_strv_length (strv),
                     sizeof (char *),
                     sort_strv,
                     NULL);
}

static void
gtk_inspector_css_node_tree_get_node_value (GtkTreeModelCssNode *model,
                                            GtkCssNode          *node,
                                            int                  column,
                                            GValue              *value)
{
  char **strv;
  char *s;

  switch (column)
    {
    case COLUMN_NAME:
      g_value_set_string (value, gtk_css_node_get_name (node));
      break;

    case COLUMN_TYPE:
      g_value_set_string (value, g_type_name (gtk_css_node_get_widget_type (node)));
      break;

    case COLUMN_VISIBLE:
      g_value_set_boolean (value, gtk_css_node_get_visible (node));
      break;

    case COLUMN_CLASSES:
      strv = gtk_css_node_get_classes (node);
      strv_sort (strv);
      s = g_strjoinv (" ", strv);
      g_value_take_string (value, s);
      g_strfreev (strv);
      break;

    case COLUMN_ID:
      g_value_set_string (value, gtk_css_node_get_id (node));
      break;

    default:
      g_assert_not_reached ();
      break;
    }
}

static void
gtk_inspector_css_node_tree_init (GtkInspectorCssNodeTree *cnt)
{
  GtkInspectorCssNodeTreePrivate *priv;
  
  cnt->priv = gtk_inspector_css_node_tree_get_instance_private (cnt);
  gtk_widget_init_template (GTK_WIDGET (cnt));
  priv = cnt->priv;

  priv->model = gtk_tree_model_css_node_new (gtk_inspector_css_node_tree_get_node_value,
                                             N_COLUMNS,
                                             G_TYPE_STRING,
                                             G_TYPE_STRING,
                                             G_TYPE_BOOLEAN,
                                             G_TYPE_STRING,
                                             G_TYPE_STRING);
  gtk_tree_view_set_model (GTK_TREE_VIEW (priv->tree_view), priv->model);
  g_object_unref (priv->model);
}

void
gtk_inspector_css_node_tree_set_object (GtkInspectorCssNodeTree *cnt,
                                        GObject                 *object)
{
  GtkInspectorCssNodeTreePrivate *priv;
  const gchar *title;

  g_return_if_fail (GTK_INSPECTOR_IS_CSS_NODE_TREE (cnt));

  priv = cnt->priv;

  title = (const gchar *)g_object_get_data (object, "gtk-inspector-object-title");
  gtk_label_set_label (GTK_LABEL (priv->object_title), title);

  if (!GTK_IS_WIDGET (object))
    {
      gtk_tree_model_css_node_set_root_node (GTK_TREE_MODEL_CSS_NODE (priv->model), NULL);
      return;
    }

  gtk_tree_model_css_node_set_root_node (GTK_TREE_MODEL_CSS_NODE (priv->model),
                                         gtk_widget_get_css_node (GTK_WIDGET (object)));
}

// vim: set et sw=2 ts=2:
