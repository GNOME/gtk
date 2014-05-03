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
#include "parasite.h"

enum
{
  COLUMN_OBJECT_NAME,
  NUM_COLUMNS
};

struct _ParasiteObjectHierarchyPrivate
{
  GtkTreeStore *model;
  GtkTreeView *tree;
};

G_DEFINE_TYPE_WITH_PRIVATE (ParasiteObjectHierarchy, parasite_objecthierarchy, GTK_TYPE_BOX)

static void
parasite_objecthierarchy_init (ParasiteObjectHierarchy *oh)
{
  oh->priv = parasite_objecthierarchy_get_instance_private (oh);
}

static void
constructed (GObject *object)
{
  ParasiteObjectHierarchy *oh = PARASITE_OBJECTHIERARCHY (object);
  GtkWidget *sw;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;

  g_object_set (object,
                "orientation", GTK_ORIENTATION_VERTICAL,
                NULL);

  sw = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                     "expand", TRUE,
                     NULL);
  gtk_container_add (GTK_CONTAINER (object), sw);

  oh->priv->model = gtk_tree_store_new (NUM_COLUMNS,
                                        G_TYPE_STRING);   // COLUMN_OBJECT_NAME
  oh->priv->tree = GTK_TREE_VIEW (gtk_tree_view_new_with_model (GTK_TREE_MODEL (oh->priv->model)));
  gtk_container_add (GTK_CONTAINER (sw), GTK_WIDGET (oh->priv->tree));


  renderer = gtk_cell_renderer_text_new ();
  g_object_set (renderer, "scale", TREE_TEXT_SCALE, NULL);
  column = gtk_tree_view_column_new_with_attributes ("Object Hierarchy", renderer,
                                                     "text", COLUMN_OBJECT_NAME,
                                                     NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (oh->priv->tree), column);
}

static void
parasite_objecthierarchy_class_init (ParasiteObjectHierarchyClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed  = constructed;
}

GtkWidget *
parasite_objecthierarchy_new (void)
{
  return GTK_WIDGET (g_object_new (PARASITE_TYPE_OBJECTHIERARCHY,
                                   NULL));
}

void
parasite_objecthierarchy_set_object (ParasiteObjectHierarchy *oh, GObject *object)
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
  while ((klass = g_type_class_peek_parent (klass))) ;
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
  gtk_tree_selection_select_iter (gtk_tree_view_get_selection (oh->priv->tree),
                                  &iter);
}

// vim: set et sw=4 ts=4:
