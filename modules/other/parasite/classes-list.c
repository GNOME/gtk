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

enum
{
  COLUMN_ENABLED,
  COLUMN_NAME,
  COLUMN_STYLE
};

typedef struct
{
  gboolean enabled;
  PangoStyle style;
} GtkInspectorClassesListByContext;

struct _GtkInspectorClassesListPrivate
{
  GtkWidget *toolbar;
  GtkWidget *view;
  GtkTreeViewColumn *column;
  GtkCellRenderer *name_renderer;
  GtkListStore *model;
  GHashTable *contexts;
  GtkStyleContext *current_context;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorClassesList, gtk_inspector_classes_list, GTK_TYPE_BOX)

static void
enabled_toggled (GtkCellRendererToggle   *renderer,
                 const gchar             *path,
                 GtkInspectorClassesList *cl)
{
  GtkTreeIter iter;
  gboolean enabled;
  GHashTable *context;
  GtkInspectorClassesListByContext *c;
  gchar *name;

  if (!gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (cl->priv->model), &iter, path))
    {
      g_warning ("GtkInspector: Couldn't find the css class path for %s.", path);
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
            gtk_style_context_add_class (cl->priv->current_context, name);
          else
            gtk_style_context_remove_class (cl->priv->current_context, name);
        }
      else
        g_warning ("GtkInspector: Couldn't find the css class %s in the class hash table.", name);
    }
  else
    g_warning ("GtkInspector: Couldn't find the hash table for the style context for css class %s.", name);
}

static void
add_clicked (GtkButton               *button,
             GtkInspectorClassesList *cl)
{
  GtkWidget *dialog, *content_area, *entry;

  dialog = gtk_dialog_new_with_buttons ("New class",
                                        GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (cl))),
                                         GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_USE_HEADER_BAR,
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

          GtkInspectorClassesListByContext *c = g_new0 (GtkInspectorClassesListByContext, 1);
          c->enabled = TRUE;
          c->style = PANGO_STYLE_ITALIC;
          g_hash_table_insert (context, (gpointer)g_strdup (name), c);

          gtk_list_store_append (cl->priv->model, &tree_iter);
          gtk_list_store_set (cl->priv->model, &tree_iter,
                             COLUMN_ENABLED, TRUE,
                             COLUMN_NAME, name,
                             COLUMN_STYLE, PANGO_STYLE_ITALIC,
                            -1);
        }
    }

  gtk_widget_destroy (dialog);
}

static void
read_classes_from_style_context (GtkInspectorClassesList *cl)
{
  GList *l, *classes;
  GtkInspectorClassesListByContext *c;
  GtkTreeIter tree_iter;
  GHashTable *hash_context;

  hash_context = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  classes = gtk_style_context_list_classes (cl->priv->current_context);

  for (l = classes; l; l = l->next)
    {
      c = g_new0 (GtkInspectorClassesListByContext, 1);
      c->enabled = TRUE;
      g_hash_table_insert (hash_context, g_strdup (l->data), c);

      gtk_list_store_append (cl->priv->model, &tree_iter);
      gtk_list_store_set (cl->priv->model, &tree_iter,
                          COLUMN_ENABLED, TRUE,
                          COLUMN_NAME, l->data,
                          COLUMN_STYLE, PANGO_STYLE_NORMAL,
                          -1);
    }
    g_list_free (classes);
    g_hash_table_replace (cl->priv->contexts, cl->priv->current_context, hash_context);
}

static void
restore_defaults_clicked (GtkButton           *button,
                          GtkInspectorClassesList *cl)
{
  GHashTableIter hash_iter;
  gchar *name;
  GtkInspectorClassesListByContext *c;
  GHashTable *hash_context = g_hash_table_lookup (cl->priv->contexts, cl->priv->current_context);

  g_hash_table_iter_init (&hash_iter, hash_context);
  while (g_hash_table_iter_next (&hash_iter, (gpointer *)&name, (gpointer *)&c))
    {
      if (c->style == PANGO_STYLE_ITALIC)
        gtk_style_context_remove_class (cl->priv->current_context, name);
      else if (!c->enabled)
        gtk_style_context_add_class (cl->priv->current_context, name);
    }

  gtk_list_store_clear (cl->priv->model);
  read_classes_from_style_context (cl);
}

static void
gtk_inspector_classes_list_init (GtkInspectorClassesList *cl)
{
  cl->priv = gtk_inspector_classes_list_get_instance_private (cl);
  gtk_widget_init_template (GTK_WIDGET (cl));
  cl->priv->contexts = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, (GDestroyNotify)g_hash_table_destroy);
}

void
gtk_inspector_classes_list_set_widget (GtkInspectorClassesList *cl,
                                       GtkWidget               *widget)
{
  GtkStyleContext *widget_context;
  GHashTable *hash_context;
  GtkTreeIter tree_iter;
  GtkInspectorClassesListByContext *c;

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
                             COLUMN_STYLE, c->style,
                            -1);
        }
    }
  else
    {
      read_classes_from_style_context (cl);
    }
}

static void
gtk_inspector_classes_list_class_init (GtkInspectorClassesListClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/inspector/classes-list.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorClassesList, toolbar);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorClassesList, view);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorClassesList, model);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorClassesList, column);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorClassesList, name_renderer);
  gtk_widget_class_bind_template_callback (widget_class, add_clicked);
  gtk_widget_class_bind_template_callback (widget_class, restore_defaults_clicked);
  gtk_widget_class_bind_template_callback (widget_class, enabled_toggled);
}

GtkWidget *
gtk_inspector_classes_list_new (void)
{
  return GTK_WIDGET (g_object_new (GTK_TYPE_INSPECTOR_CLASSES_LIST, NULL));
}

// vim: set et sw=2 ts=2:
