/*
 * Copyright (c) 2013 Intel Corporation
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

#include "object-hierarchy.h"

enum
{
  COLUMN_OBJECT_NAME
};

struct _GtkInspectorObjectHierarchyPrivate
{
  GtkTreeStore *model;
  GtkTreeView *tree;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorObjectHierarchy, gtk_inspector_object_hierarchy, GTK_TYPE_BOX)

static void
gtk_inspector_object_hierarchy_init (GtkInspectorObjectHierarchy *oh)
{
  oh->priv = gtk_inspector_object_hierarchy_get_instance_private (oh);
  gtk_widget_init_template (GTK_WIDGET (oh));
}

static void
gtk_inspector_object_hierarchy_class_init (GtkInspectorObjectHierarchyClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/inspector/object-hierarchy.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorObjectHierarchy, model);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorObjectHierarchy, tree);
}

GtkWidget *
gtk_inspector_object_hierarchy_new (void)
{
  return GTK_WIDGET (g_object_new (GTK_TYPE_INSPECTOR_OBJECT_HIERARCHY, NULL));
}

void
gtk_inspector_object_hierarchy_set_object (GtkInspectorObjectHierarchy *oh,
                                           GObject                     *object)
{
  GObjectClass *klass = G_OBJECT_GET_CLASS (object);
  const gchar *class_name;
  GtkTreeIter iter, parent;
  GSList *list = NULL, *l;

  gtk_tree_store_clear (oh->priv->model);

  do
    {
      class_name = G_OBJECT_CLASS_NAME (klass);
      list = g_slist_append (list, (gpointer)class_name);
    }
  while ((klass = g_type_class_peek_parent (klass)));
  list = g_slist_reverse (list);

  for (l = list; l; l = l->next)
    {
      gtk_tree_store_append (oh->priv->model, &iter, l == list ? NULL : &parent);
      gtk_tree_store_set (oh->priv->model, &iter,
                          COLUMN_OBJECT_NAME, l->data,
                          -1);
      parent = iter;
    }

  g_slist_free (list);

  gtk_tree_view_expand_all (oh->priv->tree);
  gtk_tree_selection_select_iter (gtk_tree_view_get_selection (oh->priv->tree), &iter);
}

// vim: set et sw=2 ts=2:
