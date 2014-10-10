/*
 * Copyright (c) 2014 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include <glib/gi18n-lib.h>

#include "resource-list.h"

#include "gtklabel.h"
#include "gtkstack.h"
#include "gtktextbuffer.h"
#include "gtktreestore.h"
#include "gtktreeselection.h"

enum
{
  COLUMN_NAME,
  COLUMN_PATH,
  COLUMN_COUNT,
  COLUMN_SIZE
};

struct _GtkInspectorResourceListPrivate
{
  GtkTreeStore *model;
  GtkTextBuffer *buffer;
  GtkWidget *image;
  GtkWidget *content;
  GtkWidget *type;
  GtkWidget *type_label;
  GtkWidget *count;
  GtkWidget *count_label;
  GtkWidget *size_label;
  GtkWidget *info_grid;
  GtkTreeViewColumn *count_column;
  GtkCellRenderer *count_renderer;
  GtkTreeViewColumn *size_column;
  GtkCellRenderer *size_renderer;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorResourceList, gtk_inspector_resource_list, GTK_TYPE_BOX)

static void
load_resources_recurse (GtkInspectorResourceList *sl,
                        GtkTreeIter              *parent,
                        const gchar              *path,
                        gint                     *count_out,
                        gsize                    *size_out)
{
  gchar **names;
  gint i;
  GtkTreeIter iter;

  names = g_resources_enumerate_children (path, 0, NULL);
  for (i = 0; names[i]; i++)
    {
      gint len;
      gchar *p;
      gboolean has_slash;
      gint count;
      gsize size;

      p = g_strconcat (path, names[i], NULL);

      len = strlen (names[i]);
      has_slash = names[i][len - 1] == '/';

      if (has_slash)
        names[i][len - 1] = '\0';

      gtk_tree_store_append (sl->priv->model, &iter, parent);
      gtk_tree_store_set (sl->priv->model, &iter,
                          COLUMN_NAME, names[i],
                          COLUMN_PATH, p,
                          -1);

      count = 0;
      size = 0;

      if (has_slash)
        {
          load_resources_recurse (sl, &iter, p, &count, &size);
          *count_out += count;
          *size_out += size;
        }
      else
        {
          count = 0;
          g_resources_get_info (p, 0, &size, NULL, NULL);
          *count_out += 1;
          *size_out += size;
        }

      gtk_tree_store_set (sl->priv->model, &iter,
                          COLUMN_COUNT, count,
                          COLUMN_SIZE, size,
                          -1);

      g_free (p);
    }
  g_strfreev (names);

}

static void
selection_changed (GtkTreeSelection         *selection,
                   GtkInspectorResourceList *rl)
{
  GtkTreeIter iter;

  if (gtk_tree_selection_get_selected (selection, NULL, &iter))
    {
      gchar *path;
      gchar *name;
      GBytes *bytes;
      gchar *type;
      gconstpointer data;
      gint count;
      gsize size;
      GError *error = NULL;

      gtk_widget_hide (rl->priv->info_grid);

      gtk_tree_model_get (GTK_TREE_MODEL (rl->priv->model), &iter,
                          COLUMN_PATH, &path,
                          COLUMN_NAME, &name,
                          COLUMN_COUNT, &count,
                          COLUMN_SIZE, &size,
                          -1);

      if (g_str_has_suffix (path, "/"))
        {
          gchar *text;

          text = g_strdup_printf ("%d", count);
          gtk_label_set_text (GTK_LABEL (rl->priv->count_label), text);
          g_free (text);

          text = g_format_size (size);
          gtk_label_set_text (GTK_LABEL (rl->priv->size_label), text);
          g_free (text);
       
          gtk_widget_hide (rl->priv->type);
          gtk_widget_hide (rl->priv->type_label);
          gtk_widget_show (rl->priv->count);
          gtk_widget_show (rl->priv->count_label);
          gtk_widget_show (rl->priv->info_grid);

          gtk_text_buffer_set_text (rl->priv->buffer, "", -1);
          gtk_stack_set_visible_child_name (GTK_STACK (rl->priv->content), "text");
          goto out;
        }
      bytes = g_resources_lookup_data (path, 0, &error);
      if (bytes == NULL)
        {
          gtk_text_buffer_set_text (rl->priv->buffer, error->message, -1);
          g_error_free (error);
          gtk_stack_set_visible_child_name (GTK_STACK (rl->priv->content), "text");
        }
      else
        {
          gchar *text;

          data = g_bytes_get_data (bytes, &size);
          type = g_content_type_guess (name, data, size, NULL);

          text = g_content_type_get_description (type);
          gtk_label_set_text (GTK_LABEL (rl->priv->type_label), text);
          g_free (text);

          text = g_format_size (size);
          gtk_label_set_text (GTK_LABEL (rl->priv->size_label), text);
          g_free (text);
       
          gtk_widget_show (rl->priv->type);
          gtk_widget_show (rl->priv->type_label);
          gtk_widget_hide (rl->priv->count);
          gtk_widget_hide (rl->priv->count_label);
          gtk_widget_show (rl->priv->info_grid);
          
          if (g_content_type_is_a (type, "text/*"))
            {
              gtk_text_buffer_set_text (rl->priv->buffer, data, -1);
              gtk_stack_set_visible_child_name (GTK_STACK (rl->priv->content), "text");
            }
          else if (g_content_type_is_a (type, "image/*"))
            {
              gtk_image_set_from_resource (GTK_IMAGE (rl->priv->image), path);
              gtk_stack_set_visible_child_name (GTK_STACK (rl->priv->content), "image");
            }
          else
            {
              gtk_text_buffer_set_text (rl->priv->buffer, "", 0);
              gtk_stack_set_visible_child_name (GTK_STACK (rl->priv->content), "text");
            }

          g_free (type);
          g_bytes_unref (bytes);
        }
out:
      g_free (path);
      g_free (name);
    }
  else
    {
      gtk_text_buffer_set_text (rl->priv->buffer, "", -1);
      gtk_stack_set_visible_child_name (GTK_STACK (rl->priv->content), "text");
    }
}

static void
load_resources (GtkInspectorResourceList *sl)
{
  gint count = 0;
  gsize size = 0;

  load_resources_recurse (sl, NULL, "/", &count, &size);
}

static void
count_data_func (GtkTreeViewColumn *col,
                 GtkCellRenderer   *cell,
                 GtkTreeModel      *model,
                 GtkTreeIter       *iter,
                 gpointer           data)
{
  gint count;
  gchar *text;

  gtk_tree_model_get (model, iter, COLUMN_COUNT, &count, -1);
  if (count > 0)
    {
      text = g_strdup_printf ("%d", count);
      g_object_set (cell, "text", text, NULL);
      g_free (text);
    }
  else
    g_object_set (cell, "text", "", NULL);
}

static void
size_data_func (GtkTreeViewColumn *col,
                GtkCellRenderer   *cell,
                GtkTreeModel      *model,
                GtkTreeIter       *iter,
                gpointer           data)
{
  gint size;
  gchar *text;

  gtk_tree_model_get (model, iter, COLUMN_SIZE, &size, -1);
  text = g_format_size (size);
  g_object_set (cell, "text", text, NULL);
  g_free (text);
}

static void
gtk_inspector_resource_list_init (GtkInspectorResourceList *sl)
{
  sl->priv = gtk_inspector_resource_list_get_instance_private (sl);
  gtk_widget_init_template (GTK_WIDGET (sl));
  gtk_tree_view_column_set_cell_data_func (sl->priv->count_column,
                                           sl->priv->count_renderer,
                                           count_data_func, sl, NULL);
  gtk_tree_view_column_set_cell_data_func (sl->priv->size_column,
                                           sl->priv->size_renderer,
                                           size_data_func, sl, NULL);
  load_resources (sl);
}

static void
gtk_inspector_resource_list_class_init (GtkInspectorResourceListClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/inspector/resource-list.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorResourceList, model);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorResourceList, buffer);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorResourceList, content);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorResourceList, image);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorResourceList, type_label);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorResourceList, type);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorResourceList, count_label);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorResourceList, count);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorResourceList, size_label);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorResourceList, info_grid);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorResourceList, count_column);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorResourceList, count_renderer);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorResourceList, size_column);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorResourceList, size_renderer);

  gtk_widget_class_bind_template_callback (widget_class, selection_changed);
}

// vim: set et sw=2 ts=2:
