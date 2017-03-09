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
#include "gtksearchbar.h"
#include "gtksearchentry.h"
#include "treewalk.h"

enum
{
  PROP_0,
  PROP_BUTTONS
};

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
  GtkWidget *name_label;
  GtkWidget *type;
  GtkWidget *type_label;
  GtkWidget *size_label;
  GtkWidget *info_grid;
  GtkWidget *stack;
  GtkWidget *tree;
  GtkWidget *buttons;
  GtkWidget *open_details_button;
  GtkWidget *close_details_button;
  GtkTreeViewColumn *path_column;
  GtkTreeViewColumn *count_column;
  GtkCellRenderer *count_renderer;
  GtkTreeViewColumn *size_column;
  GtkCellRenderer *size_renderer;
  GtkWidget *search_bar;
  GtkWidget *search_entry;
  GtkTreeWalk *walk;
  gint search_length;
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
  guint64 stored_size;

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
          if (g_resources_get_info (p, 0, &size, NULL, NULL))
            {
              *count_out += 1;
              *size_out += size;
            }
        }

      stored_size = size;
      gtk_tree_store_set (sl->priv->model, &iter,
                          COLUMN_COUNT, count,
                          COLUMN_SIZE, stored_size,
                          -1);

      g_free (p);
    }
  g_strfreev (names);

}

static gboolean
populate_details (GtkInspectorResourceList *rl,
                  GtkTreePath              *tree_path)
{
  GtkTreeIter iter;
  gchar *path;
  gchar *name;
  GBytes *bytes;
  gchar *type;
  gconstpointer data;
  gint count;
  gsize size;
  guint64 stored_size;
  GError *error = NULL;
  gchar *markup;


  gtk_tree_model_get_iter (GTK_TREE_MODEL (rl->priv->model), &iter, tree_path);
 
  gtk_tree_model_get (GTK_TREE_MODEL (rl->priv->model), &iter,
                      COLUMN_PATH, &path,
                      COLUMN_NAME, &name,
                      COLUMN_COUNT, &count,
                      COLUMN_SIZE, &stored_size,
                      -1);
  size = stored_size;

   if (g_str_has_suffix (path, "/"))
     {
       g_free (path);
       g_free (name);
       return FALSE;
     }

  markup = g_strconcat ("<span face='Monospace' size='small'>", path, "</span>", NULL);
  gtk_label_set_markup (GTK_LABEL (rl->priv->name_label), markup);
  g_free (markup);

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
      gchar *content_image;
      gchar *content_text;

      content_image = g_content_type_from_mime_type ("image/*");
      content_text = g_content_type_from_mime_type ("text/*");

      data = g_bytes_get_data (bytes, &size);
      type = g_content_type_guess (name, data, size, NULL);

      text = g_content_type_get_description (type);
      gtk_label_set_text (GTK_LABEL (rl->priv->type_label), text);
      g_free (text);

      text = g_format_size (size);
      gtk_label_set_text (GTK_LABEL (rl->priv->size_label), text);
      g_free (text);

      if (g_content_type_is_a (type, content_text))
        {
          gtk_text_buffer_set_text (rl->priv->buffer, data, -1);
          gtk_stack_set_visible_child_name (GTK_STACK (rl->priv->content), "text");
        }
      else if (g_content_type_is_a (type, content_image))
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

      g_free (content_image);
      g_free (content_text);
    }

  g_free (path);
  g_free (name);

  return TRUE;
}

static void
row_activated (GtkTreeView              *treeview,
               GtkTreePath              *path,
               GtkTreeViewColumn        *column,
               GtkInspectorResourceList *sl)
{
  if (!populate_details (sl, path))
    return;

  gtk_stack_set_visible_child_name (GTK_STACK (sl->priv->stack), "details");
  gtk_stack_set_visible_child_name (GTK_STACK (sl->priv->buttons), "details");
}

static gboolean
can_show_details (GtkInspectorResourceList *rl)
{
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeIter iter;
  gchar *path;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (rl->priv->tree));
  if (!gtk_tree_selection_get_selected (selection, &model, &iter))
    return FALSE;

  gtk_tree_model_get (GTK_TREE_MODEL (rl->priv->model), &iter,
                      COLUMN_PATH, &path,
                      -1);

   if (g_str_has_suffix (path, "/"))
     {
       g_free (path);
       return FALSE;
     }

  g_free (path);
  return TRUE;
}

static void
on_selection_changed (GtkTreeSelection         *selection,
                      GtkInspectorResourceList *rl)
{
  GtkTreeIter iter;

  if (gtk_tree_selection_get_selected (selection, NULL, &iter))
    gtk_tree_walk_reset (rl->priv->walk, &iter);
  else
    gtk_tree_walk_reset (rl->priv->walk, NULL);

  gtk_widget_set_sensitive (rl->priv->open_details_button, can_show_details (rl));
}

static void
open_details (GtkWidget                *button,
              GtkInspectorResourceList *sl)
{
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkTreePath *path;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (sl->priv->tree));
  if (!gtk_tree_selection_get_selected (selection, &model, &iter))
    return;
  
  path = gtk_tree_model_get_path (model, &iter);
  if (populate_details (sl, path))
    {
      gtk_stack_set_visible_child_name (GTK_STACK (sl->priv->stack), "details");
      gtk_stack_set_visible_child_name (GTK_STACK (sl->priv->buttons), "details");
    }

  gtk_tree_path_free (path);
}

static void
close_details (GtkWidget                *button,
               GtkInspectorResourceList *sl)
{
  gtk_stack_set_visible_child_name (GTK_STACK (sl->priv->stack), "list");
  gtk_stack_set_visible_child_name (GTK_STACK (sl->priv->buttons), "list");
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
  gsize size;
  guint64 stored_size;
  gchar *text;

  gtk_tree_model_get (model, iter, COLUMN_SIZE, &stored_size, -1);
  size = stored_size;
  text = g_format_size (size);
  g_object_set (cell, "text", text, NULL);
  g_free (text);
}

static void
on_map (GtkWidget *widget)
{
  GtkInspectorResourceList *sl = GTK_INSPECTOR_RESOURCE_LIST (widget);

  gtk_tree_view_expand_all (GTK_TREE_VIEW (sl->priv->tree));
  gtk_stack_set_visible_child_name (GTK_STACK (sl->priv->stack), "list");
  gtk_widget_set_sensitive (sl->priv->open_details_button, can_show_details (sl));
}

static void
move_search_to_row (GtkInspectorResourceList *sl,
                    GtkTreeIter              *iter)
{
  GtkTreeSelection *selection;
  GtkTreePath *path;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (sl->priv->tree));
  path = gtk_tree_model_get_path (GTK_TREE_MODEL (sl->priv->model), iter);
  gtk_tree_view_expand_to_path (GTK_TREE_VIEW (sl->priv->tree), path);
  gtk_tree_selection_select_path (selection, path);
  gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (sl->priv->tree), path, NULL, TRUE, 0.5, 0.0);
  gtk_tree_path_free (path);
}

static gboolean
key_press_event (GtkWidget                *window,
                 GdkEvent                 *event,
                 GtkInspectorResourceList *sl)
{
  if (gtk_widget_get_mapped (GTK_WIDGET (sl)))
    {
      GdkModifierType default_accel;
      gboolean search_started;

      search_started = gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (sl->priv->search_bar));
      default_accel = gtk_widget_get_modifier_mask (GTK_WIDGET (sl), GDK_MODIFIER_INTENT_PRIMARY_ACCELERATOR);

      if (search_started &&
          (event->key.keyval == GDK_KEY_Return ||
           event->key.keyval == GDK_KEY_ISO_Enter ||
           event->key.keyval == GDK_KEY_KP_Enter))
        {
          GtkTreeSelection *selection;
          GtkTreeModel *model;
          GtkTreeIter iter;
          GtkTreePath *path;

          selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (sl->priv->tree));
          if (gtk_tree_selection_get_selected (selection, &model, &iter))
            {
              path = gtk_tree_model_get_path (model, &iter);
              gtk_tree_view_row_activated (GTK_TREE_VIEW (sl->priv->tree),
                                           path,
                                           sl->priv->path_column);
              gtk_tree_path_free (path);

              return GDK_EVENT_STOP;
            }
          else
            return GDK_EVENT_PROPAGATE;
        }
      else if (search_started &&
               (event->key.keyval == GDK_KEY_Escape))
        {
          gtk_search_bar_set_search_mode (GTK_SEARCH_BAR (sl->priv->search_bar), FALSE);
          return GDK_EVENT_STOP;
        }
      else if (search_started &&
               ((event->key.state & (default_accel | GDK_SHIFT_MASK)) == (default_accel | GDK_SHIFT_MASK)) &&
               (event->key.keyval == GDK_KEY_g || event->key.keyval == GDK_KEY_G))
        {
          GtkTreeIter iter;
          if (gtk_tree_walk_next_match (sl->priv->walk, TRUE, TRUE, &iter))
            move_search_to_row (sl, &iter);
          else
            gtk_widget_error_bell (GTK_WIDGET (sl));

          return GDK_EVENT_STOP;
        }
      else if (search_started &&
               ((event->key.state & (default_accel | GDK_SHIFT_MASK)) == default_accel) &&
               (event->key.keyval == GDK_KEY_g || event->key.keyval == GDK_KEY_G))
        {
          GtkTreeIter iter;

          if (gtk_tree_walk_next_match (sl->priv->walk, TRUE, FALSE, &iter))
            move_search_to_row (sl, &iter);
          else
            gtk_widget_error_bell (GTK_WIDGET (sl));

          return GDK_EVENT_STOP;
        }

      return gtk_search_bar_handle_event (GTK_SEARCH_BAR (sl->priv->search_bar), event);
    }
  else
    return GDK_EVENT_PROPAGATE;
}

static void
on_hierarchy_changed (GtkWidget *widget,
                      GtkWidget *previous_toplevel)
{
  if (previous_toplevel)
    g_signal_handlers_disconnect_by_func (previous_toplevel, key_press_event, widget);
  g_signal_connect (gtk_widget_get_toplevel (widget), "key-press-event",
                    G_CALLBACK (key_press_event), widget);
}

static void
on_search_changed (GtkSearchEntry           *entry,
                   GtkInspectorResourceList *sl)
{
  GtkTreeIter iter;
  gint length;
  gboolean backwards;

  length = strlen (gtk_entry_get_text (GTK_ENTRY (entry)));
  backwards = length < sl->priv->search_length;
  sl->priv->search_length = length;

  if (length == 0)
    return;

  if (gtk_tree_walk_next_match (sl->priv->walk, backwards, backwards, &iter))
    move_search_to_row (sl, &iter);
  else if (!backwards)
    gtk_widget_error_bell (GTK_WIDGET (sl));
}

static gboolean
match_string (const gchar *string,
              const gchar *text)
{
  gchar *lower;
  gboolean match = FALSE;

  if (string)
    {
      lower = g_ascii_strdown (string, -1);
      match = g_str_has_prefix (lower, text);
      g_free (lower);
    }

  return match;
}

static gboolean
match_row (GtkTreeModel *model,
           GtkTreeIter  *iter,
           gpointer      data)
{
  GtkInspectorResourceList *sl = data;
  gchar *name, *path;
  const gchar *text;
  gboolean match;

  text = gtk_entry_get_text (GTK_ENTRY (sl->priv->search_entry));
  gtk_tree_model_get (model, iter,
                      COLUMN_NAME, &name,
                      COLUMN_PATH, &path,
                      -1);

  match = (match_string (name, text) ||
           match_string (path, text));

  g_free (name);
  g_free (path);

  return match;
}

static void
search_mode_changed (GObject                  *search_bar,
                     GParamSpec               *pspec,
                     GtkInspectorResourceList *sl)
{
  if (!gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (search_bar)))
    {
      gtk_tree_walk_reset (sl->priv->walk, NULL);
      sl->priv->search_length = 0;
    }
}

static void
next_match (GtkButton                *button,
            GtkInspectorResourceList *sl)
{
  if (gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (sl->priv->search_bar)))
    {
      GtkTreeIter iter;

      if (gtk_tree_walk_next_match (sl->priv->walk, TRUE, FALSE, &iter))
        move_search_to_row (sl, &iter);
      else
        gtk_widget_error_bell (GTK_WIDGET (sl));
    }
}

static void
previous_match (GtkButton                *button,
                GtkInspectorResourceList *sl)
{
  if (gtk_search_bar_get_search_mode (GTK_SEARCH_BAR (sl->priv->search_bar)))
    {
      GtkTreeIter iter;

      if (gtk_tree_walk_next_match (sl->priv->walk, TRUE, TRUE, &iter))
        move_search_to_row (sl, &iter);
      else
        gtk_widget_error_bell (GTK_WIDGET (sl));
    }
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

  g_signal_connect (sl, "map", G_CALLBACK (on_map), NULL);

  gtk_search_bar_connect_entry (GTK_SEARCH_BAR (sl->priv->search_bar),
                                GTK_ENTRY (sl->priv->search_entry));

  g_signal_connect (sl->priv->search_bar, "notify::search-mode-enabled",
                    G_CALLBACK (search_mode_changed), sl);
  sl->priv->walk = gtk_tree_walk_new (GTK_TREE_MODEL (sl->priv->model), match_row, sl, NULL);
}

static void
constructed (GObject *object)
{
  GtkInspectorResourceList *rl = GTK_INSPECTOR_RESOURCE_LIST (object);

  g_signal_connect (rl->priv->open_details_button, "clicked",
                    G_CALLBACK (open_details), rl);
  g_signal_connect (rl->priv->close_details_button, "clicked",
                    G_CALLBACK (close_details), rl);
  
  load_resources (rl);
}

static void
get_property (GObject    *object,
              guint       param_id,
              GValue     *value,
              GParamSpec *pspec)
{
  GtkInspectorResourceList *rl = GTK_INSPECTOR_RESOURCE_LIST (object);

  switch (param_id)
    {
    case PROP_BUTTONS:
      g_value_take_object (value, rl->priv->buttons);
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
  GtkInspectorResourceList *rl = GTK_INSPECTOR_RESOURCE_LIST (object);
  
  switch (param_id)
    {
    case PROP_BUTTONS:
      rl->priv->buttons = g_value_get_object (value);
      rl->priv->open_details_button = gtk_stack_get_child_by_name (GTK_STACK (rl->priv->buttons), "list");
      rl->priv->close_details_button = gtk_stack_get_child_by_name (GTK_STACK (rl->priv->buttons), "details");
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
finalize (GObject *object)
{
  GtkInspectorResourceList *sl = GTK_INSPECTOR_RESOURCE_LIST (object);

  gtk_tree_walk_free (sl->priv->walk);

  G_OBJECT_CLASS (gtk_inspector_resource_list_parent_class)->finalize (object);
}

static void
gtk_inspector_resource_list_class_init (GtkInspectorResourceListClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = get_property;
  object_class->set_property = set_property;
  object_class->constructed = constructed;
  object_class->finalize = finalize;

  g_object_class_install_property (object_class, PROP_BUTTONS,
      g_param_spec_object ("buttons", NULL, NULL,
                           GTK_TYPE_WIDGET, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/resource-list.ui");
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorResourceList, model);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorResourceList, buffer);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorResourceList, content);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorResourceList, image);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorResourceList, name_label);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorResourceList, type_label);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorResourceList, type);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorResourceList, size_label);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorResourceList, info_grid);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorResourceList, count_column);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorResourceList, count_renderer);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorResourceList, size_column);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorResourceList, size_renderer);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorResourceList, stack);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorResourceList, tree);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorResourceList, search_bar);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorResourceList, search_entry);
  gtk_widget_class_bind_template_child_private (widget_class, GtkInspectorResourceList, path_column);

  gtk_widget_class_bind_template_callback (widget_class, row_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_selection_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_hierarchy_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_search_changed);
  gtk_widget_class_bind_template_callback (widget_class, next_match);
  gtk_widget_class_bind_template_callback (widget_class, previous_match);
}

// vim: set et sw=2 ts=2:
