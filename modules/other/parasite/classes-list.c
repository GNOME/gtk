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

#include "classes-list.h"
#include "parasite.h"

enum
{
  COLUMN_ENABLED,
  COLUMN_NAME,
  COLUMN_USER,
  NUM_COLUMNS
};

typedef struct
{
  gboolean enabled;
  gboolean user;
} ParasiteClassesListByContext;

struct _ParasiteClassesListPrivate
{
  GtkWidget *toolbar;
  GtkWidget *view;
  GtkListStore *model;
  GHashTable *contexts;
  GtkStyleContext *current_context;
};

G_DEFINE_TYPE_WITH_PRIVATE (ParasiteClassesList, parasite_classeslist, GTK_TYPE_BOX)

static void
enabled_toggled (GtkCellRendererToggle *renderer, gchar *path, ParasiteClassesList *cl)
{
  GtkTreeIter iter;
  gboolean enabled;
  GHashTable *context;
  ParasiteClassesListByContext *c;
  gchar *name;

  if (!gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (cl->priv->model), &iter, path))
    {
      g_warning ("Parasite: Couldn't find the css class path for %s.", path);
      return;
    }

  gtk_tree_model_get (GTK_TREE_MODEL (cl->priv->model), &iter,
                      COLUMN_ENABLED, &enabled,
                      COLUMN_NAME, &name,
                      -1);
  enabled = !enabled;
  gtk_list_store_set (cl->priv->model, &iter,
                      COLUMN_ENABLED, enabled,
                      -1);

  context = g_hash_table_lookup (cl->priv->contexts, cl->priv->current_context);
  if (context)
    {
      c = g_hash_table_lookup (context, name);
      if (c)
        {
          c->enabled = enabled;
          if (enabled)
            {
              gtk_style_context_add_class (cl->priv->current_context, name);
            }
          else
            {
              gtk_style_context_remove_class (cl->priv->current_context, name);
            }
        }
      else
        {
          g_warning ("Parasite: Couldn't find the css class %s in the class hash table.", name);
        }
    }
  else
    {
      g_warning ("Parasite: Couldn't find the hash table for the style context for css class %s.", name);
    }
}

static void
add_clicked (GtkButton *button, ParasiteClassesList *cl)
{
  GtkWidget *dialog, *content_area, *entry;

  dialog = gtk_dialog_new_with_buttons ("New class",
                                         GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (cl))),
                                         GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                         "_OK", GTK_RESPONSE_OK,
                                         "Cancel", GTK_RESPONSE_CANCEL,
                                         NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

  entry = g_object_new (GTK_TYPE_ENTRY,
                        "visible", TRUE,
                        "margin", 5,
                        "placeholder-text", "Class name",
                        "activates-default", TRUE,
                        NULL);
  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  gtk_container_add (GTK_CONTAINER (content_area), entry);

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
    {
      const gchar *name = gtk_entry_get_text (GTK_ENTRY (entry));
      GHashTable *context = g_hash_table_lookup (cl->priv->contexts, cl->priv->current_context);

      if (*name && !g_hash_table_contains (context, name))
        {
          GtkTreeIter tree_iter;

          gtk_style_context_add_class (cl->priv->current_context, name);

          ParasiteClassesListByContext *c = g_new0 (ParasiteClassesListByContext, 1);
          c->enabled = TRUE;
          c->user = TRUE;
          g_hash_table_insert (context, (gpointer)g_strdup (name), c);

          gtk_list_store_append (cl->priv->model, &tree_iter);
          gtk_list_store_set (cl->priv->model, &tree_iter,
                             COLUMN_ENABLED, TRUE,
                             COLUMN_NAME, name,
                             COLUMN_USER, TRUE,
                            -1);
        }
    }

  gtk_widget_destroy (dialog);
}

static void
read_classes_from_style_context (ParasiteClassesList *cl)
{
  GList *l, *classes;
  ParasiteClassesListByContext *c;
  GtkTreeIter tree_iter;
  GHashTable *hash_context;

  hash_context = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  classes = gtk_style_context_list_classes (cl->priv->current_context);

  for (l = classes; l; l = l->next)
    {
      c = g_new0 (ParasiteClassesListByContext, 1);
      c->enabled = TRUE;
      g_hash_table_insert (hash_context, g_strdup (l->data), c);

      gtk_list_store_append (cl->priv->model, &tree_iter);
      gtk_list_store_set (cl->priv->model, &tree_iter,
                          COLUMN_ENABLED, TRUE,
                          COLUMN_NAME, l->data,
                          COLUMN_USER, FALSE,
                          -1);
    }
    g_list_free (classes);
    g_hash_table_replace (cl->priv->contexts, cl->priv->current_context, hash_context);
}

static void
restore_defaults_clicked (GtkButton *button, ParasiteClassesList *cl)
{
  GHashTableIter hash_iter;
  gchar *name;
  ParasiteClassesListByContext *c;
  GHashTable *hash_context = g_hash_table_lookup (cl->priv->contexts, cl->priv->current_context);

  g_hash_table_iter_init (&hash_iter, hash_context);
  while (g_hash_table_iter_next (&hash_iter, (gpointer *)&name, (gpointer *)&c))
    {
      if (c->user)
        {
          gtk_style_context_remove_class (cl->priv->current_context, name);
        }
      else if (!c->enabled)
        {
          gtk_style_context_add_class (cl->priv->current_context, name);
        }
    }

  gtk_list_store_clear (cl->priv->model);
  read_classes_from_style_context (cl);
}

static void
create_toolbar (ParasiteClassesList *cl)
{
  GtkWidget *button;

  cl->priv->toolbar = g_object_new (GTK_TYPE_TOOLBAR,
                                    "icon-size", GTK_ICON_SIZE_SMALL_TOOLBAR,
                                    "sensitive", FALSE,
                                    NULL);
  gtk_container_add (GTK_CONTAINER (cl), cl->priv->toolbar);

  button = g_object_new (GTK_TYPE_TOOL_BUTTON,
                         "icon-name", "add",
                         "tooltip-text", "Add a class",
                         NULL);
  g_signal_connect (button, "clicked", G_CALLBACK (add_clicked), cl);
  gtk_container_add (GTK_CONTAINER (cl->priv->toolbar), button);

  button = g_object_new (GTK_TYPE_TOOL_BUTTON,
                         "icon-name", "revert",
                         "tooltip-text", "Restore defaults for this widget",
                         NULL);
  g_signal_connect (button, "clicked", G_CALLBACK (restore_defaults_clicked), cl);
  gtk_container_add (GTK_CONTAINER (cl->priv->toolbar), button);
}

static void
draw_name_column (GtkTreeViewColumn   *column,
                  GtkCellRenderer     *renderer,
                  GtkTreeModel        *model,
                  GtkTreeIter         *iter,
                  ParasiteClassesList *cl)
{
  gboolean user;

  gtk_tree_model_get (model, iter, COLUMN_USER, &user, -1);
  if (user)
    {
      g_object_set (renderer, "style", PANGO_STYLE_ITALIC, NULL);
    }
  else
    {
      g_object_set (renderer, "style", PANGO_STYLE_NORMAL, NULL);
    }
}

static void
parasite_classeslist_init (ParasiteClassesList *cl)
{
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  GtkWidget *sw;

  g_object_set (cl, "orientation", GTK_ORIENTATION_VERTICAL, NULL);

  cl->priv = parasite_classeslist_get_instance_private (cl);

  create_toolbar (cl);

  sw = g_object_new (GTK_TYPE_SCROLLED_WINDOW,
                     "expand", TRUE,
                     NULL);
  gtk_container_add (GTK_CONTAINER (cl), sw);

  cl->priv->contexts = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, (GDestroyNotify)g_hash_table_destroy);
  cl->priv->model = gtk_list_store_new (NUM_COLUMNS,
                                        G_TYPE_BOOLEAN,  // COLUMN_ENABLED
                                        G_TYPE_STRING,   // COLUMN_NAME
                                        G_TYPE_BOOLEAN); // COLUMN_USER
  cl->priv->view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (cl->priv->model));
  gtk_container_add (GTK_CONTAINER (sw), cl->priv->view);

  renderer = gtk_cell_renderer_toggle_new ();
  g_signal_connect (renderer, "toggled", G_CALLBACK (enabled_toggled), cl);
  column = gtk_tree_view_column_new_with_attributes ("", renderer,
                                                     "active", COLUMN_ENABLED,
                                                     NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (cl->priv->view), column);

  renderer = gtk_cell_renderer_text_new ();
  g_object_set (renderer, "scale", TREE_TEXT_SCALE, NULL);
  column = gtk_tree_view_column_new_with_attributes ("Name", renderer,
                                                     "text", COLUMN_NAME,
                                                     NULL);
  gtk_tree_view_column_set_cell_data_func (column, renderer, (GtkTreeCellDataFunc)draw_name_column, cl, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (cl->priv->view), column);
}

void
parasite_classeslist_set_widget (ParasiteClassesList *cl,
                                 GtkWidget *widget)
{
  GtkStyleContext *widget_context;
  GHashTable *hash_context;
  GtkTreeIter tree_iter;
  ParasiteClassesListByContext *c;

  gtk_list_store_clear (cl->priv->model);

  gtk_widget_set_sensitive (GTK_WIDGET (cl), TRUE);
  widget_context = gtk_widget_get_style_context (widget);

  cl->priv->current_context = widget_context;
  gtk_widget_set_sensitive (cl->priv->toolbar, TRUE);

  hash_context = g_hash_table_lookup (cl->priv->contexts, widget_context);
  if (hash_context)
    {
      GHashTableIter hash_iter;
      gchar *name;

      g_hash_table_iter_init (&hash_iter, hash_context);
      while (g_hash_table_iter_next (&hash_iter, (gpointer *)&name, (gpointer *)&c))
        {
          gtk_list_store_append (cl->priv->model, &tree_iter);
          gtk_list_store_set (cl->priv->model, &tree_iter,
                             COLUMN_ENABLED, c->enabled,
                             COLUMN_NAME, name,
                             COLUMN_USER, c->user,
                            -1);
        }
    }
  else
    {
      read_classes_from_style_context (cl);
    }
}

static void
parasite_classeslist_class_init (ParasiteClassesListClass *klass)
{
}

GtkWidget *
parasite_classeslist_new ()
{
    return GTK_WIDGET (g_object_new (PARASITE_TYPE_CLASSESLIST, NULL));
}

// vim: set et sw=4 ts=4:
